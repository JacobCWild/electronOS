################################################################################
#
# electronos-desktop — Graphical desktop environment
#
################################################################################

ELECTRONOS_DESKTOP_VERSION = 1.0.0
ELECTRONOS_DESKTOP_SITE = $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../src/desktop
ELECTRONOS_DESKTOP_SITE_METHOD = local
ELECTRONOS_DESKTOP_LICENSE = MIT
ELECTRONOS_DESKTOP_DEPENDENCIES = sdl2 sdl2_image sdl2_ttf libdrm dejavu

define ELECTRONOS_DESKTOP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define ELECTRONOS_DESKTOP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/electronos-desktop \
		$(TARGET_DIR)/usr/bin/electronos-desktop
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_ELECTRONOS_PATH)/../../configs/security/apparmor/electronos-desktop \
		$(TARGET_DIR)/etc/apparmor.d/electronos-desktop 2>/dev/null || true
endef

$(eval $(generic-package))
