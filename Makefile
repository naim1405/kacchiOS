# Makefile for kacchiOS
BUILD_DIR = build
CROSS ?=
CC = $(CROSS)gcc
LD = $(CROSS)ld
AS = $(CROSS)as

CFLAGS = -m32 -ffreestanding -O0 -g -Wall -Wextra -nostdinc \
         -fno-builtin -fno-stack-protector -I.
ASFLAGS = --32
LDFLAGS = -m elf_i386

SRC_OBJS = boot.o context.o kernel.o serial.o string.o src/memory.o src/process.o src/scheduler.o
OBJS = $(addprefix $(BUILD_DIR)/, $(SRC_OBJS))

all: $(BUILD_DIR)/kernel.elf

objs: $(OBJS)

$(BUILD_DIR)/kernel.elf: $(OBJS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T link.ld -o $@ $^

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR) $(BUILD_DIR)/src
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

run: $(BUILD_DIR)/kernel.elf
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf -m 64M -serial stdio -display none

run-vga: $(BUILD_DIR)/kernel.elf
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf -m 64M -serial mon:stdio

debug: $(BUILD_DIR)/kernel.elf
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf -m 64M -serial stdio -display none -s -S &
	@echo "Waiting for GDB connection on port 1234..."
	@echo "In another terminal run: gdb -ex 'target remote localhost:1234' -ex 'symbol-file $(BUILD_DIR)/kernel.elf'"

clean:
ifeq ($(OS),Windows_NT)
	-rmdir /S /Q $(BUILD_DIR) 2>NUL
else
	rm -rf $(BUILD_DIR)
endif

.PHONY: all run run-vga debug clean
