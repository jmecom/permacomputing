# Ember Bootstrap Design

## Goal

Design an **ember-preservable bootstrap seed** that can bring a controllable machine from near-zero state into a higher-level software environment.

The emphasis here is **not** the final OS. The emphasis is the **first rung**:

* bytes that can survive on paper
* manually transcribed or machine-read
* injected into RAM/flash through minimal means
* able to establish a tiny execution environment
* able to receive and verify a larger second-stage payload
* able to hand off to some richer runtime later

This is a project about the **reanimation path**, not the full operating system.

---

## Design scope

This document focuses primarily on the **ember** and the immediate bootstrap chain:

1. Printed representation
2. Manual or assisted transcription
3. Tiny architecture-specific stage-0 seed
4. Minimal transport protocol for stage-1
5. Handoff contract into a later runtime

Out of scope for now:

* final OS choice
* rich drivers
* full filesystem design
* full self-hosting toolchain
* broad device support strategy

---

## Threat / failure model

Assume a future where:

* online repositories may be unavailable
* package managers may be gone
* vendor tooling may be missing
* documentation may be incomplete
* only partial hardware access is available
* storage media may be damaged or absent
* power, bandwidth, and compute may be constrained

Assume the operator may still have:

* printed paper documents
* basic wiring tools
* a debugger/programmer path (JTAG, SWD, UART bootstrap, SPI programmer, etc.)
* the ability to halt a CPU, write memory, and set the program counter

Core assumption:

> If a machine can be halted, written, and resumed, it can potentially be reclaimed.

---

## System overview

The bootstrap ladder is:

```text
Ember
  -> manual / assisted entry
  -> stage-0 seed in memory
  -> stage-0 establishes tiny execution environment
  -> stage-0 receives stage-1 payload
  -> stage-1 verifies / loads runtime payload
  -> handoff into higher-level OS or environment
```

Important distinction:

* **Ember** is tiny and architecture-specific.
* **Stage-1** is still small, but no longer paper-sized.
* **Higher-level OS** is intentionally left unspecified.

---

## First design principle

The ember should do **as little as possible**.

It should **not** try to:

* boot a full OS directly
* parse complex file formats
* understand storage devices broadly
* provide a shell
* be portable as a binary across architectures

It should only:

1. establish a known machine state
2. expose one minimal communication path
3. receive a larger payload
4. verify enough integrity to avoid obvious corruption
5. transfer control

In one line:

> The ember is a tiny bridge from inert hardware to uploadable software.

---

## Ember requirements

The paper format should be:

* **human transcribable**
* **easy to visually verify**
* **damage tolerant**
* **friendly to OCR / camera capture when available**
* **line-localized**, so a single transcription error is easy to isolate
* **architecture-labeled and unambiguous**

### Non-goals for the paper format

* highest-density possible encoding
* pretty typography
* compactness at any cost
* dependence on QR-only recovery

The system should still work if the operator is typing hex by hand.

---

## Candidate paper format

For each seed image, print a block like:

* architecture
* machine profile
* version
* load address
* entry address
* total length
* whole-image checksum
* line-by-line payload rows
* line checksums
* operator instructions

### Example shape

```text
ARCH: ARMV7-M
PROFILE: GENERIC-CORTEX-M-SRAM
VERSION: 1
LOAD: 0x20000000
ENTRY: 0x20000001
LEN: 0x00000120
HASH32: 7A13C942

0000: B508 4A1D 4B1E 4900 ... | C1A2
0010: 6810 6018 1D12 429A ... | 72F0
0020: ...                    | 9034
...
```

### Why fixed-width hex

Fixed-width hex is likely the best first format because it is:

* easy to print
* easy to read aloud
* easy to type
* architecture-neutral at representation level
* already natural for low-level debugging

Base64 is denser but visually worse.

---

## Redundancy strategy

A useful ember should assume transcription mistakes.

Recommended redundancy:

* per-line checksum
* whole-image checksum
* page number and image ID on every page
* optional duplicate print with different grouping
* optional parity or Reed-Solomon style recovery in a future version

### v1 recommendation

Keep v1 simple:

* per-line CRC or small checksum
* whole-image CRC32
* no advanced ECC yet

The point is correctness with low conceptual overhead.

---

## Seed granularity

A seed image should be extremely small.

Target order of magnitude:

* hundreds of bytes if possible
* low single-digit kilobytes at absolute most

This is because paper is expensive in operator attention.

A good v1 seed should fit on:

* one page ideally
* two pages at most for a single architecture/profile

If it takes many pages, the design is probably doing too much.

---

## Architecture model

The project should distinguish between:

### ISA seed

nArchitecture-specific machine code blob for a CPU family.

Examples:

* x86_64
* aarch64
* ARMv7-M
* RV32I
* RV64I
* MIPS32

### Platform profile

Machine-specific execution assumptions.

Examples:

* loadable RAM region
* stack location
* UART base address
* MMIO details
* boot constraints

So the real unit is not just "architecture".
It is:

```text
(seed ISA blob) + (platform profile)
```

This avoids pretending one universal machine-code seed can boot every board in an ISA family without adaptation.

---

## Stage-0 responsibilities

Stage-0 is the tiny machine-code seed represented on paper.

Responsibilities:

1. establish a valid stack
2. clear or normalize minimal CPU state if needed
3. initialize one communication path
4. receive a stage-1 payload
5. write it into memory
6. compute basic integrity check
7. jump to stage-1 entrypoint

That is all.

### Strong non-goals

Stage-0 should not:

* implement a general monitor
* expose a shell
* parse storage
* do dynamic memory allocation
* support many transports at once
* perform fancy decompression

---

## Communication path

Stage-0 should prefer one simple ingress path.

Good candidates:

* UART
* debugger-assisted memory write then jump
* SPI receive path on certain profiles
* semihosting-like debugger channel on some targets

### Best default

For many practical targets, UART is the best v1 default because it is:

* simple
* common
* debuggable
* easy to bridge from many tools

But the design should allow profiles where stage-0 is entered through JTAG/SWD memory write and stage-1 is also loaded by debugger rather than UART.

---

## Stage-1 responsibilities

Stage-1 is no longer paper-sized.
It is uploaded through stage-0.

Responsibilities:

* expose a tiny monitor or loader API
* support larger payload transfer
* support memory map discovery where feasible
* verify payloads more strongly
* optionally support flash writes
* prepare a handoff environment for later software

Stage-1 is the first actually comfortable foothold.

### Conceptual split

* **Stage-0** = reanimation nerve
* **Stage-1** = recovery foothold

---

## Handoff contract

Since the target OS is unspecified for now, define a neutral **runtime handoff contract**.

Stage-1 should be able to hand control to a higher-level payload once these are true:

* entrypoint address known
* stack pointer valid
* memory regions known
* payload integrity verified
* one console/debug path optionally initialized
* any profile-specific boot parameters prepared

### Minimal handoff structure

Example conceptual handoff block:

```text
struct handoff_info {
  arch_id
  profile_id
  ram_base
  ram_size
  console_type
  console_addr
  payload_base
  payload_size
  flags
}
```

Stage-1 transfers control to:

```text
payload_entry(handoff_info*)
```

This avoids tying the design too early to a particular OS.

---

## Operator workflow

### Path A: full manual recovery

1. identify architecture/profile
2. transcribe stage-0 bytes from paper
3. inject bytes into memory through debugger/programmer
4. set entrypoint / PC
5. run stage-0
6. send stage-1 over chosen transport
7. send higher-level payload
8. hand off into runtime

### Path B: mixed paper + machine assistance

1. use camera/OCR or local script to reconstruct stage-0 from paper
2. verify checksums
3. inject via debugger
4. continue as above

The paper should be sufficient, but machine assistance is welcome when available.

---

## Suggested v1 constraints

To keep the project real, v1 should choose:

* one ISA
* one platform profile
* one paper encoding
* one communication path
* one integrity scheme
* one stage-1 transport protocol

Example v1:

* ISA: ARMv7-M
* profile: generic SRAM + UART profile
* encoding: fixed-width hex words + line CRC
* comms: UART receive
* stage-1 protocol: very small framed packets

That is enough to validate the concept.

---

## Tiny stage-1 protocol

Keep it boring.

Packet fields:

* type
* target address
* length
* payload bytes
* checksum

Commands:

* WRITE
* VERIFY
* JUMP
* PING
* INFO

Possible flow:

1. PING
2. WRITE chunks
3. VERIFY whole payload
4. JUMP entry

No negotiation complexity unless needed.

---

## Integrity model

v1 integrity should protect mainly against:

* transcription errors
* line swaps
* transfer corruption
* obvious wrong-image mistakes

It does not need to solve full adversarial authenticity yet.

Recommended v1:

* paper lines: per-line checksum
* seed image: CRC32 or similar whole-image checksum
* stage-1 transfer: packet checksum + whole-image checksum

Later versions may add:

* signatures
* Merkle chunking
* stronger ECC
* printed public keys / trust roots

---

## Design values

This project should optimize for:

* **preservability**
* **legibility**
* **small trusted base**
* **cross-platform conceptual consistency**
* **operator comprehensibility**
* **graceful recovery from mistakes**

Not for:

* maximum speed
* minimal byte count at all costs
* aesthetic cleverness
* broad hardware support on day one

---

## What makes the idea interesting

Most software assumes:

* storage
* network
* package repositories
* vendor tools
* rich OS support
* stable supply chains

This design starts lower.

It asks:

> What is the smallest printed artifact that can restore a path back into software?

That is the heart of the project.

---

## Open questions

1. What checksum scheme is best for manual workflows?
2. Should the paper format be word-oriented or byte-oriented?
3. How much platform information should be embedded in the seed vs the profile sheet?
4. Should stage-0 ever support more than one transport?
5. How should the operator identify the correct profile for an unknown board?
6. What is the best first target architecture?
7. Should the handoff contract be binary, textual, or both?

---

## Recommended next step

Write a v1 specification for exactly one target with these sections:

1. ISA and platform profile
2. paper encoding format
3. stage-0 binary layout
4. entry/load semantics
5. UART or debugger transport details
6. stage-1 packet format
7. handoff contract

That will force the concept into something testable.

---

## One-sentence summary

An ember bootstrap is a tiny printed, architecture-specific machine-code artifact that can be manually or mechanically reconstructed, injected into controllable hardware, and used to load a larger recovery payload that hands off into a higher-level runtime.
