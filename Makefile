# ============================================================================
# electronOS — Top-level Makefile
# ============================================================================
# Usage:
#   make setup          — Download Buildroot and prepare the build tree
#   make config         — Apply the electronOS defconfig
#   make build          — Build the full OS image
#   make menuconfig     — Open Buildroot interactive configuration
#   make clean          — Clean build artifacts
#   make flash SD=/dev/sdX — Flash image to SD card
# ============================================================================

BUILDROOT_VERSION := 2024.11.1
BUILDROOT_DIR     := build/buildroot-$(BUILDROOT_VERSION)
BUILDROOT_URL     := https://buildroot.org/downloads/buildroot-$(BUILDROOT_VERSION).tar.xz
EXTERNAL_DIR      := $(CURDIR)/buildroot/external
BOARD_DIR         := $(CURDIR)/buildroot/board/electronos
DEFCONFIG         := electronos_rpi5_defconfig
OUTPUT_DIR        := $(CURDIR)/output
IMAGE             := $(OUTPUT_DIR)/images/sdcard.img

.PHONY: all setup config build menuconfig clean flash help

all: build

# ---- Setup ----------------------------------------------------------------
setup:
	@echo "==> Downloading Buildroot $(BUILDROOT_VERSION)..."
	@mkdir -p build dl
	@if [ ! -d "$(BUILDROOT_DIR)" ]; then \
		wget -q -P dl "$(BUILDROOT_URL)" 2>/dev/null || \
		curl -sL -o "dl/buildroot-$(BUILDROOT_VERSION).tar.xz" "$(BUILDROOT_URL)"; \
		tar -xf "dl/buildroot-$(BUILDROOT_VERSION).tar.xz" -C build; \
	fi
	@echo "==> Buildroot ready at $(BUILDROOT_DIR)"

# ---- Configuration --------------------------------------------------------
config: setup
	@echo "==> Applying $(DEFCONFIG)..."
	@cp buildroot/configs/$(DEFCONFIG) $(BUILDROOT_DIR)/configs/
	$(MAKE) -C $(BUILDROOT_DIR) \
		BR2_EXTERNAL="$(EXTERNAL_DIR)" \
		O="$(OUTPUT_DIR)" \
		$(DEFCONFIG)

# ---- Build ----------------------------------------------------------------
build: config
	@echo "==> Building electronOS image..."
	$(MAKE) -C $(BUILDROOT_DIR) \
		BR2_EXTERNAL="$(EXTERNAL_DIR)" \
		O="$(OUTPUT_DIR)"
	@echo ""
	@echo "============================================="
	@echo "  electronOS image built successfully!"
	@echo "  Image: $(IMAGE)"
	@echo "============================================="

# ---- Interactive config ---------------------------------------------------
menuconfig: setup
	$(MAKE) -C $(BUILDROOT_DIR) \
		BR2_EXTERNAL="$(EXTERNAL_DIR)" \
		O="$(OUTPUT_DIR)" \
		menuconfig

linux-menuconfig: setup
	$(MAKE) -C $(BUILDROOT_DIR) \
		BR2_EXTERNAL="$(EXTERNAL_DIR)" \
		O="$(OUTPUT_DIR)" \
		linux-menuconfig

# ---- Flash to SD card -----------------------------------------------------
flash:
	@if [ -z "$(SD)" ]; then \
		echo "Error: specify target device, e.g. make flash SD=/dev/sdX"; \
		exit 1; \
	fi
	@if [ ! -f "$(IMAGE)" ]; then \
		echo "Error: image not found at $(IMAGE). Run 'make build' first."; \
		exit 1; \
	fi
	@echo "==> Flashing $(IMAGE) to $(SD)..."
	@echo "    WARNING: This will ERASE all data on $(SD)!"
	@read -p "    Continue? [y/N] " confirm && [ "$$confirm" = "y" ] || exit 1
	sudo dd if="$(IMAGE)" of="$(SD)" bs=4M status=progress conv=fsync
	sudo sync
	@echo "==> Flash complete. You can now boot your Raspberry Pi 5."

# ---- Clean ----------------------------------------------------------------
clean:
	@echo "==> Cleaning build artifacts..."
	rm -rf $(OUTPUT_DIR)

distclean: clean
	@echo "==> Removing Buildroot sources..."
	rm -rf build dl

# ---- Help -----------------------------------------------------------------
help:
	@echo "electronOS Build System"
	@echo "======================="
	@echo ""
	@echo "Targets:"
	@echo "  setup          Download Buildroot"
	@echo "  config         Apply electronOS configuration"
	@echo "  build          Build the complete OS image"
	@echo "  menuconfig     Interactive Buildroot configuration"
	@echo "  linux-menuconfig  Interactive kernel configuration"
	@echo "  flash SD=/dev/sdX  Flash image to SD card"
	@echo "  clean          Remove build output"
	@echo "  distclean      Remove everything (including downloads)"
	@echo ""
	@echo "Requirements:"
	@echo "  - Linux host (Ubuntu 22.04+ recommended)"
	@echo "  - ~15GB free disk space"
	@echo "  - Internet connection (first build only)"
