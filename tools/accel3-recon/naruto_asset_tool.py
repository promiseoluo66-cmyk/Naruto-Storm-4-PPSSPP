#!/usr/bin/env python3
"""Unified front-end for supported Naruto PSP asset workflows."""
from __future__ import annotations
import argparse, json, shutil, subprocess, sys
from pathlib import Path
from accel3_probe import inventory_iso

TITLE_RULES = {
    "accel3": ("ULJS00236", "ULUS10518", "NARUTIMATE ACCEL 3", "ULTIMATE NINJA HEROES 3"),
    "impact": ("ULTIMATE NINJA IMPACT",),
}

def read_psf_fields(data: bytes) -> dict[str, str]:
    if len(data) < 20 or data[:4] != b"\x00PSF":
        return {}
    key_table = int.from_bytes(data[8:12], "little")
    data_table = int.from_bytes(data[12:16], "little")
    count = int.from_bytes(data[16:20], "little")
    if count > 4096 or key_table >= len(data) or data_table >= len(data):
        return {}
    fields = {}
    for i in range(count):
        off = 20 + i * 16
        if off + 16 > len(data): break
        key_off = int.from_bytes(data[off:off+2], "little")
        fmt = int.from_bytes(data[off+2:off+4], "little")
        length = int.from_bytes(data[off+4:off+8], "little")
        max_len = int.from_bytes(data[off+8:off+12], "little")
        value_off = int.from_bytes(data[off+12:off+16], "little")
        ks, vs = key_table + key_off, data_table + value_off
        if ks >= len(data) or vs >= len(data) or length > max_len or length > 1048576: continue
        ke = data.find(b"\x00", ks, min(len(data), ks + 256))
        if ke < 0 or vs + length > len(data): continue
        key = data[ks:ke].decode("ascii", "replace")
        if fmt in (0x0204, 0x0004):
            fields[key] = data[vs:vs+length].split(b"\x00",1)[0].decode("utf-8","replace")
    return fields

def detect_game(iso: Path):
    report = inventory_iso(iso)
    row = next((x for x in report["entries"] if x["path"].upper().endswith("PARAM.SFO")), None)
    fields = {}
    if row:
        with iso.open("rb") as f:
            f.seek(row["extent_lba"] * 2048)
            fields = read_psf_fields(f.read(min(row["size"], 1048576)))
    identity = " ".join((fields.get("DISC_ID",""), fields.get("TITLE",""))).upper()
    for game, markers in TITLE_RULES.items():
        if any(marker in identity for marker in markers): return game, fields
    return "unknown", fields

def main():
    p = argparse.ArgumentParser(description="One front-end; separate Naruto PSP game adapters.")
    p.add_argument("iso", type=Path)
    p.add_argument("--game", choices=("auto","impact","accel3"), default="auto")
    p.add_argument("--unit", dest="unit_path")
    p.add_argument("--out", type=Path, default=Path("naruto-asset-report.json"))
    a = p.parse_args()
    if not a.iso.is_file(): p.error(f"ISO does not exist: {a.iso}")
    try:
        game, fields = detect_game(a.iso) if a.game == "auto" else (a.game,{})
        if game == "accel3":
            report = inventory_iso(a.iso)
            report["selected_adapter"] = "accel3"
            report["status"] = "reconnaissance-only"
            report["not_yet_supported"] = ["archive decryption/unpacking","model and skeleton export","animation clip export","moveset/action-table export"]
            a.out.write_text(json.dumps(report,indent=2),encoding="utf-8")
            result = {"game":game,"report":str(a.out),"entry_count":report["entry_count"]}
        elif game == "impact":
            exe = a.unit_path or shutil.which("unit")
            result = {"game":game,"adapter":"UNIT/NNL","unit_executable":exe,
              "status":"tool-found" if exe else "unit-not-found",
              "limitation":"This front-end currently checks UNIT availability/help only. A validated export command and NNL bridge are still required; no animation/moveset export is claimed."}
            if exe:
                try:
                    proc = subprocess.run([exe,"--help"],capture_output=True,text=True,timeout=15)
                    result["help_exit_code"] = proc.returncode
                    result["help_text"] = (proc.stdout+"\n"+proc.stderr)[:10000]
                except (OSError,subprocess.TimeoutExpired) as exc: result["help_error"] = str(exc)
            a.out.write_text(json.dumps(result,indent=2),encoding="utf-8")
        else:
            print("Game auto-detection failed. PARAM.SFO fields:",json.dumps(fields,ensure_ascii=False))
            print("Use --game impact or --game accel3 only after checking the disc.")
            return 3
        print(json.dumps(result,indent=2,ensure_ascii=False))
        return 0
    except (OSError,ValueError,KeyError) as exc:
        print("ERROR:",exc,file=sys.stderr); return 2

if __name__ == "__main__": raise SystemExit(main())
