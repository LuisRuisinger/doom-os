# DoomOS Makefile
# Assumes this is run inside the Dev Container / toolchain environment.

TARGET      := x86_64-elf
CROSS       := $(TARGET)-

CXX         := $(CROSS)g++
CC          := $(CROSS)gcc
AS          := $(CROSS)gcc

BUILD       := build
ISO_DIR     := $(BUILD)/iso

KERNEL_ELF  := $(BUILD)/kernel.elf
ISO         := $(BUILD)/DoomOS.iso

LINKER      := config/linker.ld
GRUB_CFG    := config/grub.cfg

# =================================================================================================
# Flags
# =================================================================================================

INCLUDES := \
	-I$(CURDIR)

CXXFLAGS := \
	-std=c++20 \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-stack-protector \
	-fno-use-cxa-atexit \
	-fno-threadsafe-statics \
	-mno-red-zone \
	-mcmodel=kernel \
	-nostdlib \
	-Wall \
	-Wextra \
	-Wpedantic \
	$(INCLUDES)

ASFLAGS := \
	-ffreestanding \
	-mno-red-zone \
	-nostdlib \
	$(INCLUDES)

LDFLAGS := \
	-nostdlib \
	-T $(LINKER) \
	-Wl,-z,max-page-size=0x1000

LIBS := \
	-lgcc

QEMUFLAGS := \
	-cdrom $(ISO) \
	-serial stdio \
	-no-reboot \
	-no-shutdown

# =================================================================================================
# Sources
# =================================================================================================

ASM_SRCS := $(shell find kernel -name '*.S' | sort)
CXX_SRCS := $(shell find kernel -name '*.cpp' | sort)

OBJS := \
	$(patsubst %.S,$(BUILD)/%.o,$(ASM_SRCS)) \
	$(patsubst %.cpp,$(BUILD)/%.o,$(CXX_SRCS))

# =================================================================================================
# Default targets
# =================================================================================================

all: iso

kernel: toolchain-check $(KERNEL_ELF)

iso: toolchain-check $(ISO)

# =================================================================================================
# Build rules
# =================================================================================================

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.S
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJS) $(LINKER)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(ISO): $(KERNEL_ELF) $(GRUB_CFG)
	grub-file --is-x86-multiboot2 $(KERNEL_ELF)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	cp $(GRUB_CFG) $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

# =================================================================================================
# Utility targets
# =================================================================================================

check-multiboot2: $(KERNEL_ELF)
	grub-file --is-x86-multiboot2 $(KERNEL_ELF)
	@echo "Multiboot2 header OK"

run: iso
	qemu-system-x86_64 $(QEMUFLAGS)

debug: iso
	qemu-system-x86_64 $(QEMUFLAGS) \
		-s \
		-S

qemu-log: iso
	qemu-system-x86_64 $(QEMUFLAGS) \
		-d int,cpu_reset,guest_errors \
		-D $(BUILD)/qemu.log

clean:
	rm -rf $(BUILD)

toolchain-check:
	@command -v $(CC) >/dev/null || { echo "Missing $(CC). Open this project in the Dev Container or install the DoomOS cross toolchain."; exit 1; }
	@command -v $(CXX) >/dev/null || { echo "Missing $(CXX). Open this project in the Dev Container or install the DoomOS cross toolchain."; exit 1; }

compdb: toolchain-check
	@command -v bear >/dev/null || { echo "Missing bear. Open this project in the Dev Container or install bear."; exit 1; }
	bear --output compile_commands.json -- $(MAKE) clean kernel

# =================================================================================================
# Phony targets
# =================================================================================================

.PHONY: \
	all \
	kernel \
	iso \
	run \
	debug \
	qemu-log \
	clean \
	toolchain-check \
	compdb \
	check-multiboot2