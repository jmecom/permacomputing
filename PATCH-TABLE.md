# UART Patch Table Design

## Goal

Allow one mostly-generic seed binary to be adapted to many boards by patching a small table of I/O constants and mode flags, instead of rewriting the whole seed.

---

## Core idea

The seed contains:

- generic recovery logic
- a fixed patch table region
- tiny I/O routines that read config from that table

The operator provides:

- seed bytes
- patch values for the target board

This keeps the seed mostly stable while moving board-specific UART details into a small, explicit data block.

---

## Design constraints

The patch table should be:

- small
- fixed-layout
- easy to patch manually or with a tool
- readable on paper
- sufficient for simple polled UART I/O
- optional, so the seed can still run in debugger-only mode

It should not try to abstract every possible serial controller. It only needs to cover a useful subset of common UART-like MMIO patterns.

---

## Patch table layout

```c
struct io_patch_table {
  u32 magic;
  u32 version;
  u32 flags;

  u32 uart_base;
  u32 uart_tx_off;
  u32 uart_rx_off;
  u32 uart_stat_off;

  u32 uart_tx_ready_mask;
  u32 uart_rx_ready_mask;

  u32 uart_tx_ready_polarity;
  u32 uart_rx_ready_polarity;
};


Field meanings
	•	magic
Identifies the patch table as present and valid.
	•	version
Patch table format version.
	•	flags
Mode bits controlling UART presence and behavior.
	•	uart_base
Base MMIO address of the UART block.
	•	uart_tx_off
Offset of transmit register from uart_base.
	•	uart_rx_off
Offset of receive register from uart_base.
	•	uart_stat_off
Offset of status register from uart_base.
	•	uart_tx_ready_mask
Bit mask used to test whether transmit is ready.
	•	uart_rx_ready_mask
Bit mask used to test whether receive data is available.
	•	uart_tx_ready_polarity
Whether TX-ready means the masked bit is set or clear.
	•	uart_rx_ready_polarity
Whether RX-ready means the masked bit is set or clear.

⸻

Seed-side interface

The seed should expose only a tiny abstract I/O layer:
	•	emit(byte)
	•	has_key()
	•	key()

These routines read the patch table and perform the appropriate MMIO operations.

That keeps the rest of the monitor logic independent of hardcoded UART addresses.

⸻

Patch table placement

The patch table should live at a fixed offset inside the seed image.

Example:
seed_base + 0x40 = patch table


This makes it easy to:
	•	document
	•	inspect
	•	patch manually
	•	validate at runtime

The fixed offset should be part of the seed ABI.

⸻

Runtime behavior

At startup, the seed:
	1.	locates the patch table at the fixed offset
	2.	validates magic
	3.	validates version
	4.	reads flags
	5.	decides whether UART mode is enabled
	6.	uses patch values for all low-level serial I/O

If validation fails, the seed should either:
	•	halt
	•	or enter debugger-only fallback mode

⸻

Example paper representation

PATCH TABLE OFFSET: 0x0040

MAGIC                0x51545442
VERSION              0x00000001
FLAGS                0x00000007
UART_BASE            0x40004000
UART_TX_OFF          0x00000000
UART_RX_OFF          0x00000004
UART_STAT_OFF        0x00000008
UART_TX_READY_MASK   0x00000020
UART_RX_READY_MASK   0x00000001
UART_TX_POLARITY     0x00000001
UART_RX_POLARITY     0x00000001


Meaning of the example flags:
	•	UART enabled
	•	RX supported
	•	TX supported

⸻

Flags

Suggested initial flag bits:
	•	bit 0: UART enabled
	•	bit 1: RX supported
	•	bit 2: TX supported
	•	bit 3: debugger-only mode
	•	bit 4: status register shared with TX/RX registers

Keep the flag set small in v1.

⸻

Validity rules

The seed should reject or fall back when:
	•	magic is wrong
	•	version is unsupported
	•	UART is enabled but uart_base == 0
	•	TX or RX is enabled but required offsets or masks are missing

This avoids jumping into obviously broken I/O behavior.

⸻

No-UART mode

The patch table must support a mode where no UART is available.

This is important because many real targets will have:
	•	unknown UART mapping
	•	inaccessible UART pins
	•	missing clock or pinmux setup
	•	no serial path worth trusting yet

In no-UART mode:
	•	flags disables UART
	•	the seed still runs
	•	interaction happens through debugger-assisted memory inspection or later patching

So the patch table is not just for UART config. It also allows the seed to admit that UART is not currently usable.

⸻

Why patching is better than separate seed variants

Without a patch table, you would need:
	•	a separate seed per board
	•	hardcoded UART assumptions in each seed
	•	much more paper duplication
	•	more porting work
	•	harder auditing

With a patch table, you get:
	•	one mostly-stable seed per ISA
	•	smaller per-board metadata
	•	cleaner documentation
	•	a more honest abstraction boundary

This is especially useful if many boards differ only in:
	•	UART base address
	•	register offsets
	•	ready-bit semantics

⸻

Limits of this design

This abstraction only works for boards where UART differences are mostly data-driven.

It may fail or become awkward when boards need:
	•	pinmux configuration
	•	clock tree setup
	•	special enable sequences
	•	FIFO setup
	•	secure-world handoff
	•	device-specific bring-up code before UART is usable

So the patch table is not a universal serial abstraction. It is a useful first cut for a broad but limited class of targets.

⸻

Extension point

If needed later, the design can grow by adding:
	•	optional init routine selector
	•	optional alternate transport type
	•	optional endianness or access-width flags
	•	optional packed mini-script for device init

But v1 should avoid this unless forced.

⸻

Recommended v1

For v1, keep the patch table limited to:
	•	validation header
	•	mode flags
	•	base address
	•	three offsets
	•	two masks
	•	two polarity values

That is enough to prove the idea without overdesigning it.

⸻

One-line summary

A UART patch table lets a mostly-generic seed binary adapt to different boards by externalizing serial-controller details into a small fixed data block, instead of baking them into separate seed variants.
