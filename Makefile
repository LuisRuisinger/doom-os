# DoomOS Makefile
# Run inside the Dev Container.

TARGET := x86_64-elf
CROSS  := $(TARGET)-

CXX  := $(CROSS)g++
AS   := $(CROSS)gcc
QEMU := qemu-system-x86_64
GDB  ?= gdb

BUILD   := build
ISO_DIR := $(BUILD)/iso

KERNEL := $(BUILD)/kernel.elf
ISO    := $(BUILD)/DoomOS.iso

LINKER   := config/linker.ld
GRUB_CFG := config/grub.cfg

GDB_HOST ?= 127.0.0.1
GDB_PORT ?= 1234

ASM_SRCS := $(shell find kernel -name '*.S' | sort)
CXX_SRCS := $(shell find kernel -name '*.cpp' | sort)

OBJS := \
	$(ASM_SRCS:%.S=$(BUILD)/%.o) \
	$(CXX_SRCS:%.cpp=$(BUILD)/%.o)

DEPS := $(OBJS:.o=.d)

INCLUDES := -I$(CURDIR)

COMMON_FLAGS := \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-mno-red-zone \
	-mcmodel=kernel \
	-nostdlib \
	-g3 \
	-O0 \
	-fno-omit-frame-pointer \
	-MMD \
	-MP \
	$(INCLUDES)

CXXFLAGS := \
	-std=c++20 \
	$(COMMON_FLAGS) \
	-fno-exceptions \
	-fno-rtti \
	-fno-use-cxa-atexit \
	-fno-threadsafe-statics \
	-Wall \
	-Wextra \
	-Wpedantic

ASFLAGS := $(COMMON_FLAGS)

LDFLAGS := \
	-nostdlib \
	-no-pie \
	-T $(LINKER) \
	-Wl,-z,max-page-size=0x1000

LIBS := -lgcc

QEMUFLAGS := \
	-cdrom $(ISO) \
	-serial stdio \
	-display none \
	-no-reboot \
	-no-shutdown

.PHONY: all kernel iso run debug qemu-gdb gdb compdb check clean

all: iso

kernel: $(KERNEL)

iso: $(ISO)

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) $(LINKER)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(ISO): $(KERNEL) $(GRUB_CFG)
	grub-file --is-x86-multiboot2 $(KERNEL)
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp $(GRUB_CFG) $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run: iso
	$(QEMU) $(QEMUFLAGS)

qemu-gdb: iso
	$(QEMU) $(QEMUFLAGS) -S -gdb tcp:$(GDB_HOST):$(GDB_PORT)

debug: qemu-gdb

gdb: $(KERNEL)
	$(GDB) $(KERNEL) -ex "target remote 127.0.0.1:$(GDB_PORT)"

compdb:
	bear --output compile_commands.json -- $(MAKE) clean kernel

check: $(KERNEL)
	grub-file --is-x86-multiboot2 $(KERNEL)
	@echo "Multiboot2 header OK"

clean:
	rm -rf $(BUILD)

-include $(DEPS)

.DELETE_ON_ERROR: