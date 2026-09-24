TOOLCHAIN ?= $(HOME)/opt/m68k-amigaos-gcc-16.2
CC       = $(TOOLCHAIN)/bin/m68k-amigaos-gcc
# MUI 3.8-Header (vendor/mui) vor denen der Toolchain: die verlangen
# muimaster V20, installiert ist 19.x. MCC-Header kommen aus der Toolchain.
CFLAGS   = -m68030 -mcrt=nix20 -Os -Wall -Wno-pointer-sign -MMD -MP -Iinclude -Ibuild -Ivendor/mui/include
LDFLAGS  = -m68030 -mcrt=nix20 -s
# Stack-Tausch aus libnix erzwingen (siehe __stack in main.c/gui.c)
LDLIBS   = -Wl,-u,___stkswap

CORE = amiloc config json net provider_openai tools shell agent ui toolchain web skills
CLI  = main $(CORE)
# muistubs.c MUSS eine eigene Uebersetzungseinheit bleiben (siehe Kopfkommentar)
GUI  = gui theme gui_agent gui_prefs gui_newproj gui_projsel gui_sessions gui_skills muistubs $(CORE)

CLI_OBJS = $(CLI:%=build/%.o)
GUI_OBJS = $(GUI:%=build/%.o)

all: build/amicode build/AmiCodeIDE build/AmiCodeIDE.info build/catalogs/deutsch/AmiCode.catalog

# Texte: eingebaut Englisch (catalogs/AmiCode.cd), Uebersetzungen in catalogs/*.ct.
# tools/locale.py erzeugt daraus die MSG_-Nummern und die .catalog-Dateien
# (ersetzt catcomp/FlexCat, die sich nicht cross bauen lassen).
build/locale_strings.h build/catalogs/deutsch/AmiCode.catalog: catalogs/AmiCode.cd $(wildcard catalogs/*.ct) tools/locale.py src/gui.c | build
	python3 tools/locale.py

$(CLI_OBJS) $(GUI_OBJS): build/locale_strings.h

build/amicode: $(CLI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(CLI_OBJS) $(LDLIBS)

build/AmiCodeIDE: $(GUI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(GUI_OBJS) $(LDLIBS)

# Workbench-Icon aus art/icon.jpeg (braucht Python mit PIL: make PYTHON=/pfad/python3)
PYTHON ?= python3

build/AmiCodeIDE.info: art/icon.jpeg tools/appicon.py | build
	$(PYTHON) tools/appicon.py

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build

# Binaries per amimcp-Protokoll auf den Amiga schieben (Stuff:AmiCode)
push: all
	python3 push.py build/amicode build/AmiCodeIDE build/AmiCodeIDE.info docs/AmiCodeIDE.guide
	python3 push.py --dest Stuff:AmiCode/Catalogs/deutsch build/catalogs/deutsch/AmiCode.catalog
	python3 push.py --dest Stuff:AmiCode/Help/deutsch docs/deutsch/AmiCodeIDE.guide
	python3 push.py --dest Stuff:AmiCode/Skills skills/*.md

# Release-Archiv (Version aus src/gui.c): build/AmiCodeIDE-<version>.lha
VERSION := $(shell sed -n 's/^\#define VERSION_STRING *"\(.*\)"/\1/p' src/gui.c)
LHA      = $(TOOLCHAIN)/bin/lha
DIST     = build/dist/AmiCodeIDE

dist: all
	rm -rf build/dist build/AmiCodeIDE-*.lha
	mkdir -p $(DIST)/Skills $(DIST)/Catalogs/deutsch $(DIST)/Help/deutsch
	cp build/AmiCodeIDE build/AmiCodeIDE.info build/amicode docs/AmiCodeIDE.guide LICENSE $(DIST)/
	cp build/catalogs/deutsch/AmiCode.catalog $(DIST)/Catalogs/deutsch/
	cp docs/deutsch/AmiCodeIDE.guide $(DIST)/Help/deutsch/
	cp skills/*.md $(DIST)/Skills/
	cd build/dist && $(LHA) aq ../AmiCodeIDE-$(VERSION).lha AmiCodeIDE
	@echo "-> build/AmiCodeIDE-$(VERSION).lha"

-include $(wildcard build/*.d)

.PHONY: all clean push dist
