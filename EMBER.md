# Ember Bootstrap Design

## Status

This repository now implements a minimal v1 ember for one concrete target:

- ISA: `ARMV7-M`
- Profile: `GENERIC-CORTEX-M-SRAM-DEBUG`
- Transport: debugger-assisted SRAM writes
- Integrity: CRC32 over the stage-1 image, CRC16-CCITT per paper row
- Paper encoding: fixed-width hex, grouped as 16-bit words

The point of this v1 is narrow: go from stage-0 source code to a Cortex-M machine-code image to a printable paper seed.

It does not try to solve universal board support, UART drivers, flash programming, or a general recovery monitor yet.

---

## Artifact Flow

```text
targets/cortex-m/{startup.S,stage0.c,linker.ld}
  -> arm-none-eabi-gcc
  -> stage0.elf
  -> objcopy
  -> stage0.bin
  -> tools/paper_seed.py
  -> stage0.paper.txt
  -> stage0.paper.pdf
```

Primary commands:

- `make paper`
- `nix build .#cortex-m-paper-seed`

---

## Concrete Target

### ISA

- ARMv7-M
- Thumb state
- linked to run from SRAM at `0x20000000`

### Platform profile

The v1 profile assumes only:

- writable SRAM starts at `0x20000000`
- at least `32 KiB` of contiguous SRAM is available
- a debugger can write memory, inspect memory, set registers, and resume execution

This is intentionally a debugger-first profile. It avoids pretending we already have a universal UART or flash path for all Cortex-M boards.

---

## Memory Layout

The current implementation uses these fixed addresses:

| Region | Address | Purpose |
| --- | --- | --- |
| stage-0 image | `0x20000000` | paper-reconstructed ember image |
| mailbox | `0x20000800` | stage-1 metadata written by the debugger |
| stage-1 payload | `0x20001000` | larger image to verify and run |
| stack top | `0x20008000` | initial MSP for stage-0 |

Stage-1 maximum payload size in this profile is `0x00004000` bytes.

---

## Stage-0 Contract

Stage-0 is a tiny SRAM-resident boot image with a Cortex-M vector table.

The operator or host tool:

1. writes the stage-0 bytes to `0x20000000`
2. sets `MSP = *(uint32_t *)0x20000000`
3. sets `PC  = *(uint32_t *)0x20000004`
4. writes the stage-1 image to `0x20001000`
5. writes the mailbox to `0x20000800`
6. resumes the CPU

Mailbox format:

```c
struct ember_mailbox {
  uint32_t magic;        // 0x31424d45 = "EMB1"
  uint32_t payload_base; // must be 0x20001000
  uint32_t payload_size; // bytes, min 8, max 0x4000
  uint32_t payload_crc32;
};
```

Stage-0 then:

1. validates the mailbox
2. computes CRC32 over the stage-1 image
3. reads the stage-1 vector table
4. checks that the new MSP and reset handler point into SRAM
5. sets `VTOR` to the stage-1 base
6. loads the new MSP
7. branches to the stage-1 reset handler

If any check fails, stage-0 enters a `bkpt` loop.

This is the minimal reanimation path implemented in the repo.

---

## Paper Encoding

The paper seed is generated from the flat `stage0.bin` image.

Header fields:

- architecture
- profile
- version
- load address
- initial MSP
- entry address
- total length
- whole-image CRC32

Payload rows:

- `16` bytes per line
- grouped as `4` hex digits per 16-bit word
- left column is byte offset within the image
- right column is CRC16-CCITT for that row

Example row shape:

```text
0010: 00F0 12F8 4FF0 0001 0021 8847 FEE7 0000 | 7A91
```

The rows encode the raw little-endian image bytes. They are not instruction-disassembly words.

---

## Files

Implementation files:

- `targets/cortex-m/startup.S`
- `targets/cortex-m/stage0.c`
- `targets/cortex-m/linker.ld`
- `targets/cortex-m/profile.json`
- `tools/paper_seed.py`
- `Makefile`

Build outputs:

- `build/cortex-m/stage0.elf`
- `build/cortex-m/stage0.bin`
- `build/cortex-m/stage0.lst`
- `build/cortex-m/stage0.paper.txt`
- `build/cortex-m/stage0.paper.pdf`

---

## Operator Workflow

### Manual or mixed recovery

1. identify the `ARMV7-M / GENERIC-CORTEX-M-SRAM-DEBUG` profile
2. reconstruct `stage0.bin` from the printed paper seed
3. inject the image at `0x20000000`
4. set `MSP` and `PC` from the first two vector words
5. inject the stage-1 image at `0x20001000`
6. compute and write the mailbox values
7. resume execution

### Machine-assisted recovery

If local tooling exists, the same binary can be reconstructed from `stage0.paper.txt` or the PDF rather than typed by hand. The printed form remains the preserved artifact.

---

## Non-Goals In This Repo Revision

This v1 does not yet implement:

- UART stage-1 transfer
- flash writes
- board auto-detection
- payload signatures
- ECC beyond row CRC and image CRC
- a generic runtime handoff structure

For now, the handoff is native Cortex-M vector-table transfer into a verified SRAM image.

---

## Next Useful Step

The next practical extension is still within Cortex-M:

- keep the same paper format
- keep the same stage-0 size discipline
- add a second profile that receives stage-1 over UART instead of debugger memory writes

That would test the same ember model without broadening architecture scope too early.
