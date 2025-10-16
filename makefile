# -----------------------------
# Makefile for simple OS
# -----------------------------
# entry.s -> .o, bootloader.s -> flat 512-byte bootloader.bin
# kernel.c -> ELF -> flat binary -> combined bootable OS

# Tools
NASM    := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy

CFLAGS  := -m32 -ffreestanding -O2 -Wall -Iinclude
LDFLAGS := -m elf_i386 -T linker.ld

# Sources
BOOT_SRC   := bootloader.s
ENTRY_SRC  := entry.s
KERNEL_SRC := kernel.c $(shell find kernel/ -name '*.c')

# Objects
OBJ_ENTRY  := $(ENTRY_SRC:.s=.o)
OBJ_KERNEL := $(KERNEL_SRC:.c=.o)
OBJ_ALL    := $(OBJ_ENTRY) $(OBJ_KERNEL)

# Binaries
KERNEL_ELF    := kernel.elf
KERNEL_BIN    := kernel.bin
BOOTLOADER_BIN := bootloader.bin
BOOTABLE_BIN  := os.bin

# -----------------------------
# Default target
# -----------------------------
all: bootable

# -----------------------------
# Compile entry + kernel
# -----------------------------
%.o: %.s
	$(NASM) -f elf32 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# Link kernel ELF
# -----------------------------
kernel: $(OBJ_ALL)
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF) $^

# Convert ELF -> flat binary
$(KERNEL_BIN): kernel
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

# -----------------------------
# Compile bootloader
# -----------------------------
bootloader: $(BOOT_SRC)
	$(NASM) -f bin $< -o $(BOOTLOADER_BIN)
	@echo "Bootloader binary created: $(BOOTLOADER_BIN)"

# -----------------------------
# Create bootable OS
# -----------------------------
bootable: bootloader $(KERNEL_BIN)
	# Combine bootloader + kernel
	cat $(BOOTLOADER_BIN) $(KERNEL_BIN) > $(BOOTABLE_BIN)
	# Add padding if needed (optional)
	dd if=/dev/zero bs=512 count=300 >> $(BOOTABLE_BIN) 2>/dev/null
	@echo "Bootable OS created: $(BOOTABLE_BIN)"

# -----------------------------
# Clean
# -----------------------------
clean:
	rm -f $(OBJ_ALL) $(KERNEL_ELF) $(KERNEL_BIN) $(BOOTLOADER_BIN) $(BOOTABLE_BIN)

.PHONY: all kernel bootloader bootable clean
