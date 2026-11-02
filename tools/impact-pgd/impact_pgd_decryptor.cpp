#include "amctrl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kPgdHeaderSize = 0x90;
constexpr std::size_t kPreviewSize = 16;

bool has_pgd_magic(const std::array<u8, kPgdHeaderSize> &header) {
    return header[0] == 0x00 && header[1] == 'P' &&
           header[2] == 'G' && header[3] == 'D';
}

std::string hex_bytes(const std::vector<u8> &bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) out << ' ';
        out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return out.str();
}

std::string format_hint(const std::vector<u8> &preview) {
    const auto starts = [&preview](const char *value, std::size_t length) {
        return preview.size() >= length &&
               std::equal(value, value + length, preview.begin());
    };
    if (starts("CFC.DIG", 7)) return "CFC.DIG signature candidate";
    if (starts("CDDATA", 6)) return "CDDATA signature candidate";
    if (starts("CPK ", 4)) return "CRIWARE CPK";
    if (starts("RIFF", 4)) return "RIFF";
    if (starts("\0PSF", 4)) return "PSF";
    return "unknown; inspect the decrypted header with NNL/Racjin";
}

std::string json_escape(const std::string &value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<unsigned int>(c);
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

void usage(const char *program) {
    std::cerr
        << "Usage: " << program << " <input.pgd> <output.bin> [--force]\n"
        << "       " << program << " --self-test\n\n"
        << "Decrypts one raw PSP PGD file using PPSSPP's libkirk/amctrl code.\n"
        << "The PGD open flag is 2, matching PPSSPP's sceIo PGD read path.\n";
}

int self_test() {
    std::array<u8, kPgdHeaderSize> header{};
    header[0] = 0x00;
    header[1] = 'P';
    header[2] = 'G';
    header[3] = 'D';

    if (!has_pgd_magic(header)) {
        std::cerr << "self-test failed: PGD magic was not recognised\n";
        return 1;
    }
    header[1] = 'X';
    if (has_pgd_magic(header)) {
        std::cerr << "self-test failed: invalid magic was accepted\n";
        return 1;
    }
    if (!pgd_valid_block_size(16) || !pgd_valid_block_size(0x100000) ||
        pgd_valid_block_size(0) || pgd_valid_block_size(24) ||
        pgd_valid_block_size(0x200000)) {
        std::cerr << "self-test failed: PGD block-size validation mismatch\n";
        return 1;
    }

    const std::vector<u8> cfc = {'C','F','C','.','D','I','G'};
    if (format_hint(cfc).find("CFC.DIG") == std::string::npos) {
        std::cerr << "self-test failed: CFC.DIG signature hint mismatch\n";
        return 1;
    }
    std::cout << "self-test: PASS (magic, block-size bounds, signature hint)\n";
    std::cout << "Note: this is a code-level test, not a real-game PGD decrypt test.\n";
    return 0;
}

int decrypt_file(const fs::path &input_path, const fs::path &output_path, bool force) {
    if (!fs::is_regular_file(input_path)) {
        std::cerr << "error: input is not a regular file: " << input_path << '\n';
        return 2;
    }

    const auto input_size = fs::file_size(input_path);
    if (input_size < kPgdHeaderSize) {
        std::cerr << "error: file is shorter than a PGD header (0x90 bytes)\n";
        return 2;
    }

    std::error_code ec;
    const fs::path canonical_input = fs::weakly_canonical(input_path, ec);
    ec.clear();
    const fs::path canonical_output = fs::weakly_canonical(output_path, ec);
    if (!ec && canonical_input == canonical_output) {
        std::cerr << "error: input and output paths must be different\n";
        return 2;
    }
    if (fs::exists(output_path) && !force) {
        std::cerr << "error: output already exists; pass --force to replace it\n";
        return 2;
    }

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "error: could not open input file\n";
        return 2;
    }

    std::array<u8, kPgdHeaderSize> header{};
    input.read(reinterpret_cast<char *>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (input.gcount() != static_cast<std::streamsize>(header.size()) ||
        !has_pgd_magic(header)) {
        std::cerr << "error: input does not begin with the raw PGD magic (\\0PGD)\n";
        return 2;
    }

    KirkState kirk{};
    if (kirk_init(&kirk) != 0) {
        std::cerr << "error: PPSSPP libkirk initialisation failed\n";
        return 3;
    }

    // PPSSPP's sceIo.cpp opens PGD data with flag 2 and an optional caller key.
    // The Impact files under investigation are raw PGD files, so no caller key
    // is supplied here. pgd_open authenticates the PGD header before data is read.
    std::unique_ptr<PGD_DESC, decltype(&pgd_close)> pgd(
        pgd_open(&kirk, header.data(), 2, nullptr), &pgd_close);
    if (!pgd) {
        std::cerr << "error: pgd_open rejected the header (PGD MAC/key check failed)\n";
        std::cerr << "This may mean the PGD uses a different key mode, or the file is not a supported PGD variant.\n";
        return 4;
    }

    if (!pgd_valid_block_size(pgd->block_size) || pgd->data_size == 0 ||
        pgd->block_buf == nullptr || pgd->block_nr == 0) {
        std::cerr << "error: PGD descriptor contains invalid size fields\n";
        return 4;
    }

    const std::uint64_t block_size = pgd->block_size;
    const std::uint64_t data_size = pgd->data_size;
    const std::uint64_t data_offset = pgd->data_offset;
    const std::uint64_t block_count = (data_size + block_size - 1) / block_size;
    const std::uint64_t stored_span = block_count * block_size;
    if (data_offset < kPgdHeaderSize ||
        block_count > pgd->block_nr ||
        data_offset > input_size ||
        stored_span > input_size - data_offset) {
        std::cerr << "error: decrypted PGD descriptor points outside the input file\n";
        std::cerr << "data_offset=" << data_offset
                  << " data_size=" << data_size
                  << " block_size=" << block_size
                  << " input_size=" << input_size << '\n';
        return 4;
    }

    fs::path temporary_path = output_path;
    temporary_path += ".part";
    if (fs::exists(temporary_path)) {
        if (!force) {
            std::cerr << "error: temporary output already exists: " << temporary_path
                      << " (remove it or pass --force)\n";
            return 2;
        }
        fs::remove(temporary_path, ec);
        if (ec) {
            std::cerr << "error: cannot remove stale temporary output: " << ec.message() << '\n';
            return 2;
        }
    }

    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "error: could not create temporary output: " << temporary_path << '\n';
        return 2;
    }

    std::vector<u8> preview;
    preview.reserve(kPreviewSize);
    std::uint64_t remaining = data_size;

    for (std::uint64_t block = 0; block < block_count; ++block) {
        const std::uint64_t source_offset = data_offset + block * block_size;
        input.clear();
        input.seekg(static_cast<std::streamoff>(source_offset), std::ios::beg);
        if (!input) {
            std::cerr << "error: could not seek to encrypted block " << block << '\n';
            output.close();
            fs::remove(temporary_path, ec);
            return 5;
        }

        input.read(reinterpret_cast<char *>(pgd->block_buf),
                   static_cast<std::streamsize>(block_size));
        if (input.gcount() != static_cast<std::streamsize>(block_size)) {
            std::cerr << "error: short read for encrypted block " << block << '\n';
            output.close();
            fs::remove(temporary_path, ec);
            return 5;
        }

        if (pgd_decrypt_block(&kirk, pgd.get(), static_cast<int>(block)) <= 0) {
            std::cerr << "error: libkirk failed to decrypt block " << block << '\n';
            output.close();
            fs::remove(temporary_path, ec);
            return 5;
        }

        const std::size_t write_size = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, block_size));
        const std::size_t preview_needed =
            preview.size() < kPreviewSize ? kPreviewSize - preview.size() : 0;
        const std::size_t preview_copy = std::min(preview_needed, write_size);
        preview.insert(preview.end(), pgd->block_buf, pgd->block_buf + preview_copy);

        output.write(reinterpret_cast<const char *>(pgd->block_buf),
                     static_cast<std::streamsize>(write_size));
        if (!output) {
            std::cerr << "error: failed writing decrypted output\n";
            output.close();
            fs::remove(temporary_path, ec);
            return 5;
        }
        remaining -= write_size;
    }

    output.flush();
    if (!output) {
        std::cerr << "error: failed flushing decrypted output\n";
        output.close();
        fs::remove(temporary_path, ec);
        return 5;
    }
    output.close();

    if (force && fs::exists(output_path)) {
        fs::remove(output_path, ec);
        if (ec) {
            std::cerr << "error: cannot replace output: " << ec.message() << '\n';
            fs::remove(temporary_path, ec);
            return 5;
        }
    }
    fs::rename(temporary_path, output_path, ec);
    if (ec) {
        std::cerr << "error: could not move completed output into place: " << ec.message() << '\n';
        fs::remove(temporary_path, ec);
        return 5;
    }

    std::cout << "{\n"
              << "  \"status\": \"pgd-decrypted\",\n"
              << "  \"input\": \"" << json_escape(input_path.string()) << "\",\n"
              << "  \"output\": \"" << json_escape(output_path.string()) << "\",\n"
              << "  \"pgd_open_flag\": 2,\n"
              << "  \"key_index\": " << pgd->key_index << ",\n"
              << "  \"drm_type\": " << pgd->drm_type << ",\n"
              << "  \"data_offset\": " << data_offset << ",\n"
              << "  \"data_size\": " << data_size << ",\n"
              << "  \"block_size\": " << block_size << ",\n"
              << "  \"blocks_decrypted\": " << block_count << ",\n"
              << "  \"first_bytes_hex\": \"" << hex_bytes(preview) << "\",\n"
              << "  \"format_hint\": \"" << json_escape(format_hint(preview)) << "\"\n"
              << "}\n";
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
        return self_test();
    }
    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 2;
    }
    const bool force = argc == 4 && std::string(argv[3]) == "--force";
    if (argc == 4 && !force) {
        usage(argv[0]);
        return 2;
    }
    try {
        return decrypt_file(fs::path(argv[1]), fs::path(argv[2]), force);
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << '\n';
        return 10;
    }
}
