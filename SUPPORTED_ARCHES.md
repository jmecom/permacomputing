# Supported Architectures

This repository is targeting these stage-0 seed families:

| Architecture | Scope | Emulator / runner in the dev shell | Tooling in the dev shell | Notes |
| --- | --- | --- | --- | --- |
| Z80 | Planned | No QEMU target | `sjasmplus` | QEMU does not provide a `qemu-system-z80` binary. |
| 6502 | Planned | No QEMU target | `cc65` (`ca65`, `ld65`), `dasm`, `acme` | QEMU does not provide a `qemu-system-6502` binary. |
| 8086 | Planned | `qemu-system-i386` | `nasm`, `clang`, `ld.lld` | We use the i386 system emulator for 16-bit real-mode bring-up. |
| x86_64 | Planned | `qemu-system-x86_64` | `clang`, `ld.lld`, `nasm`, `gdb` | Suitable for BIOS/bootloader and long-mode experiments. |
| ARMv7-M | Planned | `qemu-system-arm` | `arm-none-eabi-*`, `openocd`, `gdb` | This remains the cleanest first bootstrap target. |
| AArch64 | Planned | `qemu-system-aarch64` | `aarch64-none-elf-*`, `gdb` | Bare-metal AArch64 support is included in the shell. |
| AVR | Planned | `qemu-system-avr` | `avr-*`, `avrdude` | Useful for very small bootstrap experiments. |
| RISC-V | Planned | `qemu-system-riscv32`, `qemu-system-riscv64` | `riscv32-none-elf-*`, `riscv64-none-elf-*` | Covers both RV32 and RV64 families. |
| MIPS | Optional | `qemu-system-mips`, `qemu-system-mipsel` | QEMU only in the default shell for now | Secondary target for now. |

## Current policy

- The repository scope includes all architectures listed above.
- QEMU is installed for every architecture in that list that QEMU actually supports.
- Friendly launcher commands are installed as `qemu-i386`, `qemu-x86_64`, `qemu-arm`, `qemu-aarch64`, `qemu-avr`, `qemu-riscv32`, `qemu-riscv64`, `qemu-mips`, and `qemu-mipsel`.
- Z80 and 6502 stay in scope, but they will need non-QEMU emulation or hardware test paths later.
- The default shell includes every requested architecture except the optional MIPS compiler toolchain.
- The optional MIPS compiler toolchain is available in `nix develop .#full`.

## Shell conventions

- `CROSS_COMPILE` defaults to `arm-none-eabi-`.
- Additional prefixes are exported as `ARM_CROSS_COMPILE`, `AARCH64_CROSS_COMPILE`, `AVR_CROSS_COMPILE`, `RISCV32_CROSS_COMPILE`, and `RISCV64_CROSS_COMPILE`.
- In `nix develop .#full`, `MIPS_CROSS_COMPILE` is exported as `mips-none-elf-`.
- QEMU commands are exported as `QEMU_SYSTEM_I386`, `QEMU_SYSTEM_X86_64`, `QEMU_SYSTEM_ARM`, `QEMU_SYSTEM_AARCH64`, `QEMU_SYSTEM_AVR`, `QEMU_SYSTEM_RISCV32`, `QEMU_SYSTEM_RISCV64`, `QEMU_SYSTEM_MIPS`, and `QEMU_SYSTEM_MIPSEL`.
- `ember-env` prints a summary of the toolchains and launchers available in the shell.
