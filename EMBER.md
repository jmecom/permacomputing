Paper Seed Bootstrap Design

Goal

Design a paper-preservable bootstrap seed that can bring a controllable machine from near-zero state into a higher-level software environment.

The emphasis here is not the final OS. The emphasis is the first rung:
	•	bytes that can survive on paper
	•	manually transcribed or machine-read
	•	injected into RAM/flash through minimal means
	•	able to establish a tiny execution environment
	•	able to receive and verify a larger second-stage payload
	•	able to hand off to some richer runtime later

This is a project about the reanimation path, not the full operating system.

⸻

Design scope

This document focuses primarily on the paper seed and the immediate bootstrap chain:
	1.	Printed representation
	2.	Manual or assisted transcription
	3.	Tiny architecture-specific stage-0 seed
	4.	Minimal transport protocol for stage-1
	5.	Handoff contract into a later runtime

Out of scope for now:
	•	final OS choice
	•	rich drivers
	•	full filesystem design
	•	full self-hosting toolchain
	•	broad device support strategy

⸻

Threat / failure model

Assume a future where:
	•	online repositories may be unavailable
	•	package managers may be gone
	•	vendor tooling may be missing
	•	documentation may be incomplete
	•	only partial hardware access is available
	•	storage media may be damaged or absent
	•	power, bandwidth, and compute may be constrained

Assume the operator may still have:
	•	printed paper documents
	•	basic wiring tools
	•	a debugger/programmer path (JTAG, SWD, UART bootstrap, SPI programmer, etc.)
	•	the ability to halt a CPU, write memory, and set the program counter

Core assumption:

If a machine can be halted, written, and resumed, it can potentially be reclaimed.

⸻

System overview

The bootstrap ladder is now intentionally simplified:

Paper seed
  -> manual / assisted entry
  -> single seed blob in memory
  -> seed boots into tiny Forth-based recovery monitor
  -> recovery monitor receives larger payload
  -> handoff into higher-level OS or environment

Important distinction:
	•	The paper seed is still architecture- and profile-specific.
	•	But it is no longer split into a tiny stage-0 stub and separate stage-1 loader.
	•	The seed itself should boot directly into a minimal, useful recovery monitor.
	•	The higher-level OS remains intentionally unspecified.

⸻

First design principle

The paper seed should do as little as possible, but it should still land the operator in something immediately useful.

So instead of a separate stage-0 and stage-1, the design now assumes a single seed blob that:
	1.	establishes a known machine state
	2.	initializes one communication path
	3.	enters a tiny Forth-based recovery monitor
	4.	allows upload / verification / handoff of larger payloads

This keeps the architecture simpler and more honest.

In one line:

The paper seed is a tiny architecture-specific bootstrap that lands directly in a minimal Forth recovery monitor.

Constraint

This simplification only works if the seed stays small enough to remain realistic as a paper artifact.

So the design must enforce a brutal size budget.

⸻

Paper seed requirements

The paper format should be:
	•	human transcribable
	•	easy to visually verify
	•	damage tolerant
	•	friendly to OCR / camera capture when available
	•	line-localized, so a single transcription error is easy to isolate
	•	architecture-labeled and unambiguous

Non-goals for the paper format
	•	highest-density possible encoding
	•	pretty typography
	•	compactness at any cost
	•	dependence on QR-only recovery

The system should still work if the operator is typing hex by hand.

⸻

Candidate paper format

For each seed image, print a block like:
	•	architecture
	•	machine profile
	•	version
	•	load address
	•	entry address
	•	total length
	•	whole-image checksum
	•	line-by-line payload rows
	•	line checksums
	•	operator instructions

Example shape

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

Why fixed-width hex

Fixed-width hex is likely the best first format because it is:
	•	easy to print
	•	easy to read aloud
	•	easy to type
	•	architecture-neutral at representation level
	•	already natural for low-level debugging

Base64 is denser but visually worse.

⸻

Redundancy strategy

A useful paper seed should assume transcription mistakes.

Recommended redundancy:
	•	per-line checksum
	•	whole-image checksum
	•	page number and image ID on every page
	•	optional duplicate print with different grouping
	•	optional parity or Reed-Solomon style recovery in a future version

v1 recommendation

Keep v1 simple:
	•	per-line byte sum mod 256 plus XOR byte
	•	whole-image CRC32
	•	no advanced ECC yet

The point is correctness with low conceptual overhead.

Operator rule of thumb:
	•	S is the low 8 bits of the byte sum for a row
	•	X is the bytewise XOR for that row
	•	check each row locally while transcribing
	•	check the whole-image CRC32 after reconstructing the full seed

⸻

Seed granularity

A seed image should be extremely small.

Target order of magnitude:
	•	hundreds of bytes if possible
	•	low single-digit kilobytes at absolute most

This is because paper is expensive in operator attention.

A good v1 seed should fit on:
	•	one page ideally
	•	two pages at most for a single architecture/profile

If it takes many pages, the design is probably doing too much.

⸻

Architecture model

The project should distinguish between:

ISA seed

nArchitecture-specific machine code blob for a CPU family.

Examples:
	•	x86_64
	•	aarch64
	•	ARMv7-M
	•	RV32I
	•	RV64I
	•	MIPS32

Platform profile

Machine-specific execution assumptions.

Examples:
	•	loadable RAM region
	•	stack location
	•	UART base address
	•	MMIO details
	•	boot constraints

So the real unit is not just “architecture”.
It is:

(seed ISA blob) + (platform profile)

This avoids pretending one universal machine-code seed can boot every board in an ISA family without adaptation.

⸻

Seed responsibilities

The seed is the tiny machine-code artifact represented on paper.

Responsibilities:
	1.	establish a valid stack
	2.	clear or normalize minimal CPU state if needed
	3.	initialize one communication path
	4.	enter a tiny Forth-based recovery monitor
	5.	support receiving a larger payload
	6.	support basic integrity checking
	7.	transfer control to a later runtime

Strong non-goals

The seed should not:
	•	implement a rich shell
	•	parse storage broadly
	•	do dynamic memory allocation
	•	support many transports at once
	•	perform fancy decompression
	•	become a general OS in miniature

The recovery monitor should expose only a very small set of useful primitives.

Forth monitor rationale

A tiny Forth monitor is attractive because it is:
	•	interactive
	•	small
	•	easy to extend
	•	natural for memory inspection and hardware bring-up
	•	compatible with the Collapse OS spirit

But it must remain brutally constrained.

Communication path

Stage-0 should prefer one simple ingress path.

Good candidates:
	•	UART
	•	debugger-assisted memory write then jump
	•	SPI receive path on certain profiles
	•	semihosting-like debugger channel on some targets

Best default

For many practical targets, UART is the best v1 default because it is:
	•	simple
	•	common
	•	debuggable
	•	easy to bridge from many tools

But the design should allow profiles where stage-0 is entered through JTAG/SWD memory write and stage-1 is also loaded by debugger rather than UART.

⸻

Recovery monitor responsibilities

The recovery monitor is entered directly from the seed.
It is part of the seed design, not a separate stage.

Responsibilities:
	•	expose a tiny interactive Forth environment
	•	support memory read/write
	•	support payload upload
	•	support payload integrity checks
	•	optionally support flash write primitives on some profiles
	•	prepare a handoff environment for later software

Conceptual model
	•	Seed = architecture-specific bootstrap blob
	•	Recovery monitor = first useful foothold

The monitor should be treated as part of the seed artifact, but conceptually it is the first usable layer above raw bring-up.

Handoff contract

Since the target OS is unspecified for now, define a neutral runtime handoff contract.

Stage-1 should be able to hand control to a higher-level payload once these are true:
	•	entrypoint address known
	•	stack pointer valid
	•	memory regions known
	•	payload integrity verified
	•	one console/debug path optionally initialized
	•	any profile-specific boot parameters prepared

Minimal handoff structure

Example conceptual handoff block:

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

Stage-1 transfers control to:

payload_entry(handoff_info*)

This avoids tying the design too early to a particular OS.

⸻

Operator workflow

Path A: full manual recovery
	1.	identify architecture/profile
	2.	transcribe stage-0 bytes from paper
	3.	inject bytes into memory through debugger/programmer
	4.	set entrypoint / PC
	5.	run stage-0
	6.	send stage-1 over chosen transport
	7.	send higher-level payload
	8.	hand off into runtime

Path B: mixed paper + machine assistance
	1.	use camera/OCR or local script to reconstruct stage-0 from paper
	2.	verify checksums
	3.	inject via debugger
	4.	continue as above

The paper should be sufficient, but machine assistance is welcome when available.

⸻

Suggested v1 constraints

To keep the project real, v1 should choose:
	•	one ISA
	•	one platform profile
	•	one paper encoding
	•	one communication path
	•	one integrity scheme
	•	one tiny Forth monitor
	•	one handoff path into a larger runtime

Example v1:
	•	ISA: ARMv7-M
	•	profile: generic SRAM + UART profile
	•	encoding: fixed-width hex words + line CRC
	•	comms: UART receive
	•	monitor: tiny Forth-based recovery environment

That is enough to validate the concept.

⸻

Tiny payload protocol

Keep it boring.

Packet fields:
	•	type
	•	target address
	•	length
	•	payload bytes
	•	checksum

Commands:
	•	WRITE
	•	VERIFY
	•	JUMP
	•	PING
	•	INFO

Possible flow:
	1.	PING
	2.	WRITE chunks
	3.	VERIFY whole payload
	4.	JUMP entry

No negotiation complexity unless needed.

The protocol should be simple enough to implement using the primitives exposed by the Forth recovery monitor.

The protocol should be simple enough to implement using the primitives exposed by the Forth recovery monitor.

⸻

Integrity model

v1 integrity should protect mainly against:
	•	transcription errors
	•	line swaps
	•	transfer corruption
	•	obvious wrong-image mistakes

It does not need to solve full adversarial authenticity yet.

Recommended v1:
	•	paper lines: byte sum mod 256 plus XOR byte
	•	seed image: CRC32 or similar whole-image checksum
	•	payload transfer: packet checksum + whole-image checksum

Later versions may add:
	•	signatures
	•	Merkle chunking
	•	stronger ECC
	•	printed public keys / trust roots

⸻

Design values

This project should optimize for:
	•	preservability
	•	legibility
	•	small trusted base
	•	cross-platform conceptual consistency
	•	operator comprehensibility
	•	graceful recovery from mistakes

Not for:
	•	maximum speed
	•	minimal byte count at all costs
	•	aesthetic cleverness
	•	broad hardware support on day one

⸻

What makes the idea interesting

Most software assumes:
	•	storage
	•	network
	•	package repositories
	•	vendor tools
	•	rich OS support
	•	stable supply chains

This design starts lower.

It asks:

What is the smallest printed artifact that can restore a path back into software?

That is the heart of the project.

⸻

Open questions
	1.	What checksum scheme is best for manual workflows?
	2.	Should the paper format be word-oriented or byte-oriented?
	3.	How much platform information should be embedded in the seed vs the profile sheet?
	4.	Should stage-0 ever support more than one transport?
	5.	How should the operator identify the correct profile for an unknown board?
	6.	What is the best first target architecture?
	7.	Should the handoff contract be binary, textual, or both?

⸻

Recommended next step

Write a v1 specification for exactly one target with these sections:
	1.	ISA and platform profile
	2.	paper encoding format
	3.	seed binary layout
	4.	entry/load semantics
	5.	UART or debugger transport details
	6.	tiny Forth monitor primitives
	7.	payload packet format
	8.	handoff contract

That will force the concept into something testable.

⸻

One-sentence summary

A paper seed bootstrap is a tiny printed, architecture-specific machine-code artifact that can be manually or mechanically reconstructed, injected into controllable hardware, and used to load a larger recovery payload that hands off into a higher-level runtime.
