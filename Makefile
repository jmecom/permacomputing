TARGET := cortex-m
TARGET_DIR := targets/$(TARGET)
OUT_DIR ?= build/$(TARGET)

ARM_CROSS_COMPILE ?= arm-none-eabi-
CC := $(ARM_CROSS_COMPILE)gcc
OBJCOPY := $(ARM_CROSS_COMPILE)objcopy
OBJDUMP := $(ARM_CROSS_COMPILE)objdump
SIZE := $(ARM_CROSS_COMPILE)size
PYTHON ?= python3

CFLAGS := -mcpu=cortex-m3 -mthumb -Os -g3 -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -Wall -Wextra -Werror -std=c11
LDFLAGS := -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(OUT_DIR)/stage0.map -T $(TARGET_DIR)/linker.ld

STAGE0_ELF := $(OUT_DIR)/stage0.elf
STAGE0_BIN := $(OUT_DIR)/stage0.bin
STAGE0_LISTING := $(OUT_DIR)/stage0.lst
STAGE0_TEXT := $(OUT_DIR)/stage0.paper.txt
STAGE0_PDF := $(OUT_DIR)/stage0.paper.pdf

.PHONY: all clean paper cortex-m-paper

all: paper

paper: $(STAGE0_LISTING) $(STAGE0_TEXT) $(STAGE0_PDF)

cortex-m-paper: paper

clean:
	rm -rf build dist

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(STAGE0_ELF): $(TARGET_DIR)/startup.S $(TARGET_DIR)/stage0.c $(TARGET_DIR)/linker.ld | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TARGET_DIR)/startup.S $(TARGET_DIR)/stage0.c
	$(SIZE) $@

$(STAGE0_BIN): $(STAGE0_ELF)
	$(OBJCOPY) -O binary $< $@

$(STAGE0_LISTING): $(STAGE0_ELF)
	$(OBJDUMP) -D $< > $@

$(STAGE0_TEXT): $(STAGE0_BIN) $(TARGET_DIR)/profile.json | $(OUT_DIR)
	$(PYTHON) tools/paper_seed.py \
		--profile $(TARGET_DIR)/profile.json \
		--input $(STAGE0_BIN) \
		--text $@

$(STAGE0_PDF): $(STAGE0_BIN) $(TARGET_DIR)/profile.json | $(OUT_DIR)
	$(PYTHON) tools/paper_seed.py \
		--profile $(TARGET_DIR)/profile.json \
		--input $(STAGE0_BIN) \
		--pdf $@
