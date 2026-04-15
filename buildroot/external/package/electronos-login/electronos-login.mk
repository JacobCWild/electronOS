################################################################################
#
# electronos-login — Graphical login manager
#
################################################################################

ELECTRONOS_LOGIN_VERSION = 1.0.0
ELECTRONOS_LOGIN_SITE = $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../src/login
ELECTRONOS_LOGIN_SITE_METHOD = local
ELECTRONOS_LOGIN_LICENSE = MIT
ELECTRONOS_LOGIN_DEPENDENCIES = sdl2 sdl2_image sdl2_ttf libpam libdrm

define ELECTRONOS_LOGIN_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define ELECTRONOS_LOGIN_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/electronos-login \
		$(TARGET_DIR)/usr/bin/electronos-login
	$(INSTALL) -D -m 0644 $(@D)/electronos-login.pam \
		$(TARGET_DIR)/etc/pam.d/electronos-login
	$(INSTALL) -D -m 0644 $(@D)/electronos-login.service \
		$(TARGET_DIR)/usr/lib/systemd/system/electronos-login.service
	mkdir -p $(TARGET_DIR)/usr/share/electronos/login
	cp $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../assets/electronOSlogo.png \
		$(TARGET_DIR)/usr/share/electronos/login/logo.png
endef

$(eval $(generic-package))
