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
LDFLAGS := -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(OUT_DIR)/seed.map -T $(TARGET_DIR)/linker.ld

SEED_ELF := $(OUT_DIR)/seed.elf
SEED_BIN := $(OUT_DIR)/seed.bin
SEED_LISTING := $(OUT_DIR)/seed.lst
SEED_TEXT := $(OUT_DIR)/seed.paper.txt
SEED_PDF := $(OUT_DIR)/seed.paper.pdf

.PHONY: all clean paper cortex-m-paper

all: paper

paper: $(SEED_LISTING) $(SEED_TEXT) $(SEED_PDF)

cortex-m-paper: paper

clean:
	rm -rf build dist

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(SEED_ELF): $(TARGET_DIR)/startup.S $(TARGET_DIR)/seed.c $(TARGET_DIR)/linker.ld | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TARGET_DIR)/startup.S $(TARGET_DIR)/seed.c
	$(SIZE) $@

$(SEED_BIN): $(SEED_ELF)
	$(OBJCOPY) -O binary $< $@

$(SEED_LISTING): $(SEED_ELF)
	$(OBJDUMP) -D $< > $@

$(SEED_TEXT): $(SEED_BIN) $(TARGET_DIR)/profile.json tools/paper_seed.py | $(OUT_DIR)
	$(PYTHON) tools/paper_seed.py \
		--profile $(TARGET_DIR)/profile.json \
		--input $(SEED_BIN) \
		--text $@

$(SEED_PDF): $(SEED_BIN) $(TARGET_DIR)/profile.json tools/paper_seed.py | $(OUT_DIR)
	$(PYTHON) tools/paper_seed.py \
		--profile $(TARGET_DIR)/profile.json \
		--input $(SEED_BIN) \
		--pdf $@
