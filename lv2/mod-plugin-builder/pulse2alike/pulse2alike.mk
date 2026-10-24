######################################
#
# pulse2alike
#
######################################

# Pulse2Alike for mod-plugin-builder. Copy this folder into mod-plugin-builder/plugins/package/, then
#
#     ./build moddwarf-new pulse2alike          (or moddwarf, modduox, modduox-new; x86_64 for a desktop test build)
#     ./build moddwarf-new pulse2alike-publish  (onto a device on USB)
#
# It builds the checkout at PULSE2ALIKE_SOURCE - by default /root/source, where the MOD wiki's Docker setup mounts a
# project - with its SynthLib submodule checked out (git submodule update --init). See Docs/code-notes/p2Lv2.md §13.

PULSE2ALIKE_SOURCE ?= /root/source

PULSE2ALIKE_VERSION = local
PULSE2ALIKE_SITE = $(PULSE2ALIKE_SOURCE)
PULSE2ALIKE_SITE_METHOD = local
PULSE2ALIKE_BUNDLES = Pulse2Alike.lv2

# a toolchain other than the one the sources are checked with may warn where that one does not: not an error here
PULSE2ALIKE_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/lv2 BUILD=$(@D)/build/lv2 P2A_NO_WERROR=1

define PULSE2ALIKE_BUILD_CMDS
	$(PULSE2ALIKE_TARGET_MAKE)
endef

define PULSE2ALIKE_INSTALL_TARGET_CMDS
	$(PULSE2ALIKE_TARGET_MAKE) install DESTDIR=$(TARGET_DIR) LV2DIR=/usr/lib/lv2
endef

$(eval $(generic-package))
