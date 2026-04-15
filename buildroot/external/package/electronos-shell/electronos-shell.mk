################################################################################
#
# electronos-shell — Custom command shell
#
################################################################################

ELECTRONOS_SHELL_VERSION = 1.0.0
ELECTRONOS_SHELL_SITE = $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../src/shell
ELECTRONOS_SHELL_SITE_METHOD = local
ELECTRONOS_SHELL_LICENSE = MIT
ELECTRONOS_SHELL_DEPENDENCIES = readline

define ELECTRONOS_SHELL_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define ELECTRONOS_SHELL_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/electronos-shell \
		$(TARGET_DIR)/usr/bin/electronos-shell
	# Register as a valid login shell
	grep -qxF '/usr/bin/electronos-shell' $(TARGET_DIR)/etc/shells 2>/dev/null || \
		echo '/usr/bin/electronos-shell' >> $(TARGET_DIR)/etc/shells
endef

$(eval $(generic-package))
