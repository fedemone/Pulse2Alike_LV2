# Pulse2Alike as an LV2 plug-in - for MOD devices, and any Linux LV2 host. Docs/code-notes/p2Lv2.md.
#
#   make                  the bundle for this machine: build/lv2/Pulse2Alike.lv2
#   make dwarf            the bundle for a MOD Dwarf: build/dwarf/Pulse2Alike.lv2 (notes §13)
#   make publish          that bundle onto a MOD device on USB (MOD_DEVICE, 192.168.51.1 by default)
#   make install          this machine's bundle into $(DESTDIR)$(LV2DIR) - ~/.lv2 unless LV2DIR says otherwise
#   make ttl              the bundle's Turtle and panel template again, from the engine's tables - after any
#                         parameter change
#   make PRESETS=<sounds> any of these with Pulse 2 sound dumps in the bundle as presets: a .syx of one or many, or a
#                         folder of "NNN Name.syx" (notes §15)
#   make check            the Turtle up to date and valid, the binary loadable on a Dwarf, and the plug-in rendering
#                         as the engine does (notes §14)
#   make dwarf-check      the Dwarf binary loadable, and rendering, under qemu-aarch64 with the Dwarf's C library
#
# CC, CFLAGS and LDFLAGS from the environment are used, which is how mod-plugin-builder cross-builds it
# (lv2/mod-plugin-builder/). P2A_NO_WERROR=1 stops warnings in this project's own sources being errors.

ROOT       := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
LV2SRC     := $(ROOT)/lv2
SL         ?= $(ROOT)/SynthLib
# BUILD holds the bundle and nothing else, so it can go on an LV2_PATH as it is; WORK the rest
BUILD      ?= $(ROOT)/build/lv2
WORK       ?= $(BUILD).work
BUNDLE     := $(BUILD)/Pulse2Alike.lv2
VERSION    ?= 0.1.0
HOST_CC    ?= cc
LV2DIR     ?= $(HOME)/.lv2
MOD_DEVICE ?= 192.168.51.1

ENGINE   := $(addprefix $(ROOT)/src/,p2Dco.c p2Filter.c p2Env.c p2Lfo.c p2Drive.c p2Arp.c) \
            $(addprefix $(ROOT)/plugin/,p2Plugin.c p2Params.c p2Patch.c)
SOURCES  := $(ENGINE) $(LV2SRC)/p2Lv2.c $(LV2SRC)/p2Lv2Ports.c
LOG      := $(SL)/plugin/synthlibLog.c
TTL      := manifest.ttl Pulse2Alike.ttl modgui.ttl
# notes §16: the panel on a MOD device - the template generated, the rest by hand
GUI      := $(addprefix modgui/,icon-pulse2alike.html stylesheet-pulse2alike.css script-pulse2alike.js \
                                screenshot-pulse2alike.png thumbnail-pulse2alike.png)

INCLUDES := -I$(LV2SRC) -I$(ROOT)/src -I$(ROOT)/plugin -I$(SL)/plugin -I$(SL)/src
DEFINES  := -DP2_NO_EDITOR -DSYNTHLIB_PLUGIN_BUILD=1 -DP2A_VERSION_STRING='"$(VERSION)"'
WARN     := -Wall -Wextra
WERROR   := $(if $(P2A_NO_WERROR),,-Werror)

# -O3 ahead of the environment's flags, which may say otherwise. Hidden symbols: two plug-ins built on SynthLib in
# one host process must never bind to each other's.
BASE_CFLAGS := -O3 -std=gnu11 -fPIC -fvisibility=hidden -MMD -MP $(DEFINES) $(INCLUDES)
SO_LDFLAGS  := -shared -Wl,--no-undefined
# pthread: before glibc 2.34, pthread_once lives there (the Dwarf has 2.27)
LIBS        := -lm -lpthread

OBJ     := $(WORK)/obj
OBJECTS := $(patsubst $(ROOT)/%.c,$(OBJ)/%.o,$(SOURCES)) $(OBJ)/synthlibLog.o

.PHONY: all install publish ttl check check-ttl validate check-binary test dwarf dwarf-check clean

all: $(BUNDLE)/Pulse2Alike.so $(addprefix $(BUNDLE)/,$(TTL)) $(addprefix $(BUNDLE)/,$(GUI))

$(OBJ)/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(CC) $(BASE_CFLAGS) $(WARN) $(WERROR) $(CFLAGS) -c $< -o $@

# SynthLib's, so its warnings are not this project's errors; p2Lv2Compat.h gives it glibc's getprogname()
$(OBJ)/synthlibLog.o: $(LOG)
	@mkdir -p $(@D)
	$(CC) $(BASE_CFLAGS) $(WARN) -include $(LV2SRC)/p2Lv2Compat.h $(CFLAGS) -c $< -o $@

$(BUNDLE)/Pulse2Alike.so: $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SO_LDFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(BUNDLE)/%.ttl: $(LV2SRC)/Pulse2Alike.lv2/%.ttl
	@mkdir -p $(@D)
	cp $< $@

$(BUNDLE)/modgui/%: $(LV2SRC)/Pulse2Alike.lv2/modgui/%
	@mkdir -p $(@D)
	cp $< $@

-include $(OBJECTS:.o=.d)

install: all
	install -d $(DESTDIR)$(LV2DIR)/Pulse2Alike.lv2
	install -m 644 $(BUNDLE)/*.ttl $(DESTDIR)$(LV2DIR)/Pulse2Alike.lv2/
	install -m 755 $(BUNDLE)/Pulse2Alike.so $(DESTDIR)$(LV2DIR)/Pulse2Alike.lv2/
	install -d $(DESTDIR)$(LV2DIR)/Pulse2Alike.lv2/modgui
	install -m 644 $(addprefix $(BUNDLE)/,$(GUI)) $(DESTDIR)$(LV2DIR)/Pulse2Alike.lv2/modgui/

# ---- the Turtle, from the engine's tables (notes §5) --------------------------------------------------------------

HOST       := $(WORK)/host
GENERATOR  := $(HOST)/p2Lv2Ttl
HOST_FLAGS := -O1 -std=gnu11 $(DEFINES) $(INCLUDES)

$(HOST)/synthlibLog.o: $(LOG)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_FLAGS) $(WARN) -include $(LV2SRC)/p2Lv2Compat.h -c $< -o $@

GEN_SRC := $(LV2SRC)/p2Lv2Ports.c $(LV2SRC)/p2Lv2Gui.c $(LV2SRC)/p2Lv2Ttl.c
$(GENERATOR): $(ENGINE) $(GEN_SRC) $(LV2SRC)/p2Lv2Ports.h $(LV2SRC)/p2Lv2Gui.h $(HOST)/synthlibLog.o
	$(HOST_CC) $(HOST_FLAGS) $(WARN) $(WERROR) $(ENGINE) $(GEN_SRC) $(HOST)/synthlibLog.o -lm -lpthread -o $@

ttl: $(GENERATOR)
	$(GENERATOR) $(LV2SRC)/Pulse2Alike.lv2

PRESETS_TOOL := $(HOST)/p2Lv2Presets
PRESETS_SRC  := $(ROOT)/plugin/p2Patch.c $(ROOT)/plugin/p2Params.c $(LV2SRC)/p2Lv2Ports.c $(LV2SRC)/p2Lv2Presets.c

$(PRESETS_TOOL): $(PRESETS_SRC) $(LV2SRC)/p2Lv2Ports.h
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_FLAGS) $(WARN) $(WERROR) $(PRESETS_SRC) -lm -lpthread -o $@

# notes §15: with PRESETS the bundle's manifest is the committed one with the presets added, made again every time.
# A relative PRESETS is taken from where make was run, as a shell's PWD says, not from lv2/ where -C put it.
PRESETS_PATH = $(if $(filter /%,$(PRESETS)),$(PRESETS),$(PWD)/$(PRESETS))
ifneq ($(PRESETS),)
.PHONY: $(BUNDLE)/manifest.ttl
all: $(BUNDLE)/presets.ttl
$(BUNDLE)/presets.ttl: $(BUNDLE)/manifest.ttl
$(BUNDLE)/manifest.ttl: $(PRESETS_TOOL) $(LV2SRC)/Pulse2Alike.lv2/manifest.ttl
	@mkdir -p $(@D)
	$(PRESETS_TOOL) "$(PRESETS_PATH)" $(LV2SRC)/Pulse2Alike.lv2/manifest.ttl $(BUNDLE)
else ifneq ($(wildcard $(BUNDLE)/presets.ttl),)
# a bundle built with PRESETS before, built without them now, loses them
.PHONY: $(BUNDLE)/manifest.ttl
$(BUNDLE)/manifest.ttl: $(LV2SRC)/Pulse2Alike.lv2/manifest.ttl
	rm -f $(BUNDLE)/presets.ttl
	cp $< $@
endif

# ---- checks (notes §14) -------------------------------------------------------------------------------------------

check: check-ttl validate check-binary test

check-ttl: $(GENERATOR)
	@rm -rf $(WORK)/ttl-check && mkdir -p $(WORK)/ttl-check/modgui
	@$(GENERATOR) $(WORK)/ttl-check > /dev/null
	@for f in $(TTL) modgui/icon-pulse2alike.html; do \
	    diff -u $(LV2SRC)/Pulse2Alike.lv2/$$f $(WORK)/ttl-check/$$f > /dev/null || \
	        { echo "lv2/Pulse2Alike.lv2/$$f is out of date: run make -C lv2 ttl"; exit 1; }; \
	done
	@echo "check-ttl: the committed Turtle and panel template are the engine's"

# Against the LV2 specifications and MOD's vocabularies (lv2/test/mod.lv2 and modgui.lv2, from mod-lv2-extensions, for
# checking only), and the port change request's (lv2/test/kx-control-input-port-change-request.lv2, from KXStudio's
# LV2-Extensions, notes §8)
LV2_SCHEMAS ?= $(shell pkg-config --variable=lv2dir lv2 2>/dev/null || echo /usr/lib/lv2)
MOD_SCHEMAS ?= $(LV2SRC)/test/mod.lv2 $(LV2SRC)/test/modgui.lv2 $(LV2SRC)/test/kx-control-input-port-change-request.lv2
validate: all
	@command -v sord_validate > /dev/null || { echo "validate: no sord_validate (apt install sordi) - skipped"; exit 0; }; \
	sord_validate $$(find $(LV2_SCHEMAS) $(MOD_SCHEMAS) -name '*.ttl') $(addprefix $(BUNDLE)/,$(TTL))

# the glibc ceiling is a Dwarf's; a build for this machine may use this machine's
GLIBC_MAX ?= none
check-binary: all
	$(LV2SRC)/check-binary $(BUNDLE)/Pulse2Alike.so $(GLIBC_MAX)

# notes §14: the LV2 plug-in, driven as a host drives it, against the engine driven directly - the plug-in's own
# objects, so the two are the same machine code
TEST         := $(WORK)/test/p2Lv2Test
TEST_OBJECTS := $(filter-out $(OBJ)/lv2/p2Lv2.o,$(OBJECTS))
$(TEST): $(LV2SRC)/test/p2Lv2Test.c $(TEST_OBJECTS) $(BUNDLE)/Pulse2Alike.so
	@mkdir -p $(@D)
	$(CC) -O3 -std=gnu11 $(DEFINES) $(INCLUDES) $(WARN) $(WERROR) $(CFLAGS) $(LV2SRC)/test/p2Lv2Test.c $(TEST_OBJECTS) \
	    $(LDFLAGS) -lm -lpthread -ldl -o $@

test: $(TEST)
	$(TEST_RUNNER) $(TEST) $(BUNDLE)/Pulse2Alike.so

# ---- a MOD Dwarf, without mod-plugin-builder (notes §13) ----------------------------------------------------------

DWARF_CC      ?= aarch64-linux-gnu-gcc
DWARF_SYSROOT ?= $(ROOT)/build/sysroot-glibc-2.27-arm64
DWARF_BUILD   := $(ROOT)/build/dwarf
# mod-plugin-builder's for the Dwarf (plugins-dep/configs/moddwarf_defconfig)
DWARF_CFLAGS  := -mcpu=cortex-a35 -mtune=cortex-a35 -ffast-math -fno-finite-math-only -fomit-frame-pointer \
                 -fprefetch-loop-arrays -ftree-vectorize -funroll-loops -D__MOD_DEVICES__ -D_MOD_DEVICE_DWARF
DWARF_LDFLAGS := -Wl,-O1 -Wl,--as-needed -Wl,--strip-all
# notes §13: a Debian or Ubuntu cross compiler finds its own C library ahead of any --sysroot, so every search path is
# named: the compiler's own headers, then the sysroot's; the sysroot's start files and libraries
DWARF_SYSFLAGS = --sysroot=$(DWARF_SYSROOT) -nostdinc -isystem $(shell $(DWARF_CC) -print-file-name=include) \
                 -isystem $(DWARF_SYSROOT)/usr/include/aarch64-linux-gnu -isystem $(DWARF_SYSROOT)/usr/include \
                 -B$(DWARF_SYSROOT)/usr/lib/aarch64-linux-gnu/ -L$(DWARF_SYSROOT)/lib/aarch64-linux-gnu \
                 -L$(DWARF_SYSROOT)/usr/lib/aarch64-linux-gnu
DWARF_MAKE     = $(MAKE) -f $(LV2SRC)/Makefile BUILD=$(DWARF_BUILD) CC="$(DWARF_CC) $(DWARF_SYSFLAGS)" \
                 CFLAGS="$(DWARF_CFLAGS)" LDFLAGS="$(DWARF_LDFLAGS)" GLIBC_MAX=2.27

$(DWARF_SYSROOT)/.complete:
	$(LV2SRC)/do-glibc-sysroot $(DWARF_SYSROOT)

dwarf: $(DWARF_SYSROOT)/.complete
	$(DWARF_MAKE) all check-binary

dwarf-check: dwarf
	$(DWARF_MAKE) LDFLAGS= test TEST_RUNNER="qemu-aarch64 -L $(DWARF_SYSROOT)"

publish: dwarf
	cd $(DWARF_BUILD) && tar cz Pulse2Alike.lv2 | base64 | curl -F 'package=@-' http://$(MOD_DEVICE)/sdk/install; echo

clean:
	rm -rf $(BUILD) $(WORK) $(DWARF_BUILD) $(DWARF_BUILD).work
