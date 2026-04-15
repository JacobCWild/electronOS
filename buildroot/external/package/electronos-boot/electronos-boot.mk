################################################################################
#
# electronos-boot — Custom Plymouth boot splash theme
#
################################################################################

ELECTRONOS_BOOT_VERSION = 1.0.0
ELECTRONOS_BOOT_SITE = $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../src/boot
ELECTRONOS_BOOT_SITE_METHOD = local
ELECTRONOS_BOOT_LICENSE = MIT
ELECTRONOS_BOOT_DEPENDENCIES = plymouth

ELECTRONOS_BOOT_THEME_DIR = $(TARGET_DIR)/usr/share/plymouth/themes/electronos

define ELECTRONOS_BOOT_INSTALL_TARGET_CMDS
	mkdir -p $(ELECTRONOS_BOOT_THEME_DIR)
	cp $(BR2_EXTERNAL_ELECTRONOS_PATH)/../../assets/electronOSlogo.png \
		$(ELECTRONOS_BOOT_THEME_DIR)/logo.png
	cp $(@D)/electronos.plymouth $(ELECTRONOS_BOOT_THEME_DIR)/
	cp $(@D)/electronos.script $(ELECTRONOS_BOOT_THEME_DIR)/
	# Set as default Plymouth theme
	mkdir -p $(TARGET_DIR)/etc/plymouth
	echo "[Daemon]" > $(TARGET_DIR)/etc/plymouth/plymouthd.conf
	echo "Theme=electronos" >> $(TARGET_DIR)/etc/plymouth/plymouthd.conf
	echo "ShowDelay=0" >> $(TARGET_DIR)/etc/plymouth/plymouthd.conf
endef

$(eval $(generic-package))
