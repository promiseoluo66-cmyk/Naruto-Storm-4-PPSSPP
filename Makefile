TARGET = naruto_storm4
OBJS = src/launcher.o src/main.o src/gfx.o src/model.o src/rigged_model.o src/assets.o src/model_test.o src/gen_assets.o

INCDIR = include
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgum -lpspgu -lpspdisplay -lpspge -lpspctrl -lm
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Naruto Storm 4 PPSSPP

# Generate the original test character (OBJ -> NMDL/NMTX -> embedded C data).
# Needs python3 (Termux: pkg install python). See tools/README.md.
ASSET_GEN := $(shell sh -c 'python3 tools/make_test_character.py assets/chr/test && python3 tools/obj2nmdl.py assets/chr/test/test_character.obj assets/chr/test/test_character.nmdl && python3 tools/embed_assets.py src/gen_assets.c test_character_nmdl=assets/chr/test/test_character.nmdl test_character_nmtx=assets/chr/test/test_texture.nmtx && python3 tools/import_smd_zip.py assets/chr/test/naruto-shippuden-naruto.zip assets/chr/test/naruto_rigged' 2>&1)
$(if $(wildcard src/gen_assets.c),,$(error Asset generation failed - is python3 installed? $(ASSET_GEN)))

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# main.c keeps its own main(); rename it so launcher.c can run the model viewer first.
src/main.o: CFLAGS += -Dmain=game_main
