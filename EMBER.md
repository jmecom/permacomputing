Paper Seed Bootstrap Design

Goal

Design a paper-preservable bootstrap seed that can bring a controllable machine from near-zero state into a higher-level software environment.

The emphasis here is not the final OS. The emphasis is the first rung:
	•	bytes that can survive on paper
	•	manually transcribed or machine-read
	•	injected into RAM/flash through minimal means
	•	able to establish a tiny execution foothold
	•	able to hand off to a larger recovery payload
	•	able to eventually bootstrap a usable development environment for that machine

This is a project about the reanimation path, not the full operating system.

⸻

Design scope

This document focuses primarily on the paper seed and the immediate bootstrap chain:
	1.	Printed representation
	2.	Manual or assisted transcription
	3.	Tiny architecture-specific seed
	4.	Debugger-assisted ingress
	5.	Handoff into a larger recovery payload

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

Core clarification

The seed is not automatically a UART monitor, a shell, or a tiny OS.

The seed is:

the first paper-preserved executable foothold

That means the seed only earns its keep if it does something useful after the operator has injected bytes into memory.

Concretely, the seed should turn:

I can inject bytes with a debugger

into:

I have a repeatable target-side control contract

If a debugger can already load a larger recovery payload directly and jump to it safely, then a separate tiny pre-seed may be unnecessary. In that case, the first useful recovery payload is the seed.

So the project should not force an extra stage just because bootstraps often have one.

⸻

System overview

The bootstrap ladder is:

Paper seed
  -> manual / assisted reconstruction
  -> debugger/programmer writes seed into memory
  -> debugger/programmer may also write a larger recovery payload
  -> seed establishes a known execution foothold
  -> seed transfers control to the larger recovery payload
  -> recovery payload grows into a usable development environment

Important distinction:
	•	The seed is architecture- and profile-specific.
	•	The seed is debugger-ingressed in v1.
	•	UART is optional, not fundamental.
	•	The development environment does not need to exist inside the seed itself.

⸻

First design principle

The paper seed should do as little as possible while still making the next step reliable.

That usually means:
	1.	establish a known machine state
	2.	expose a tiny preserved control contract
	3.	accept or interpret a small amount of operator intent
	4.	transfer control to a larger recovery payload in a disciplined way

In one line:

The paper seed is the smallest preserved executable that turns debugger-ingressed bytes into a repeatable target-side foothold.

Constraint

This only works if the seed stays small enough to remain realistic as a paper artifact.

So the design must enforce a brutal size budget.

⸻

What the seed is actually doing

The seed should not be thought of as “the whole recovery environment.”

The seed is doing one of two useful jobs:

1.	Tiny verify-and-jump foothold
	•	assumes the next payload is debugger-loaded
	•	optionally checks a header or checksum
	•	transfers control in a disciplined way
	•	very small

2.	Tiny interactive foothold
	•	exposes a very small stack language or monitor
	•	supports a few primitives like read, write, verify, and jump
	•	still assumes debugger ingress for the seed itself
	•	larger, but more useful

The seed should not try to be the development environment. The larger recovery payload after the seed is where the real climb begins.

⸻

Debugger-first v1

For v1, debugger access is enough for ingress.

That means:
	•	the seed itself does not need UART in order to exist
	•	the operator can load the seed with SWD/JTAG or equivalent
	•	the operator can also load the next payload the same way
	•	the seed’s purpose is then to provide a small preserved handoff contract

This is cleaner than pretending UART exists before the system has been configured well enough to use it.

UART can still matter later, but it should not define the essence of the seed.

⸻

Paper seed requirements

The paper format should be:
	•	human transcribable
	•	easy to visually verify
	•	damage tolerant
	•	friendly to OCR / camera capture when available
	•	line-localized, so a single transcription error is easy to isolate
	•	architecture-labeled and unambiguous

Non-goals for the paper format:
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
	•	line checks
	•	operator instructions

Example shape

ARCH: ARMV7-M
PROFILE: GENERIC-CORTEX-M
VERSION: 1
LOAD: 0x20000000
ENTRY: 0x20000001
LEN: 0x00000120
HASH32: 7A13C942

0000: B508 4A1D 4B1E 4900 ... | S=9C X=70
0010: 6810 6018 1D12 429A ... | S=54 X=1F
0020: ...                    | S=34 X=A2
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

Architecture-specific machine code blob for a CPU family.

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
	•	reset-state assumptions
	•	debugger ingress assumptions
	•	optional transport assumptions

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
	3.	expose a tiny preserved control contract
	4.	optionally verify the next payload
	5.	transfer control to that payload

Strong non-goals

The seed should not:
	•	implement a rich shell
	•	parse storage broadly
	•	do dynamic memory allocation
	•	support many transports at once
	•	perform fancy decompression
	•	become a general OS in miniature
	•	try to contain the whole development environment

The larger recovery payload is where the real development foothold begins.

⸻

Communication path

The seed should assume debugger/programmer ingress in v1.

That means:
	•	the operator loads the seed through SWD/JTAG or equivalent
	•	the operator may also load the next payload the same way
	•	the seed does not need UART in order to justify its existence

Optional later transports:
	•	UART
	•	SPI
	•	semihosting-like debugger channels

But these should be treated as later convenience, not as the definition of the seed concept.

⸻

Larger recovery payload

The larger recovery payload is the first thing that should grow toward an actual development environment.

Its responsibilities may include:
	•	a richer monitor
	•	payload transfer
	•	better integrity checks
	•	flash write primitives
	•	memory map discovery
	•	eventual climb toward assembler, compiler, editor, runtime, and storage

Conceptual model:
	•	seed = smallest preserved executable foothold
	•	recovery payload = first actually comfortable foothold

This keeps the seed small and honest.

⸻

Handoff contract

Since the target OS is unspecified for now, define a neutral runtime handoff contract.

The seed should be able to hand control to a larger payload once these are true:
	•	entrypoint address known
	•	stack pointer valid
	•	memory regions known well enough
	•	payload integrity verified if verification is included
	•	any profile-specific preconditions prepared

Minimal conceptual handoff block:

struct handoff_info {
  arch_id
  profile_id
  ram_base
  ram_size
  payload_base
  payload_size
  flags
}

The seed or recovery payload transfers control to:

payload_entry(handoff_info*)

This avoids tying the design too early to a particular OS.

⸻

Operator workflow

Path A: full manual recovery
	1.	identify architecture/profile
	2.	transcribe seed bytes from paper
	3.	inject bytes into memory through debugger/programmer
	4.	verify row checks and whole-image checksum
	5.	set entrypoint / PC
	6.	run the seed
	7.	inject or load the larger recovery payload
	8.	hand off into that payload

Path B: mixed paper + machine assistance
	1.	use camera/OCR or local script to reconstruct seed bytes from paper
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
	•	debugger-first ingress
	•	one integrity scheme
	•	one tiny foothold contract
	•	one handoff path into a larger recovery payload

Example v1:
	•	ISA: ARMv7-M
	•	profile: generic SRAM + debugger profile
	•	encoding: fixed-width hex words + row sum/XOR
	•	ingress: debugger memory write
	•	seed: tiny verify-and-jump foothold or tiny stack-based control stub

That is enough to validate the concept.

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
	•	larger payload: checksum or stronger verification as needed

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

What is the smallest printed executable artifact that can restore a path back into software?

That is the heart of the project.

⸻

Open questions
	1.	How small can the seed remain while still being useful?
	2.	Should the seed be only verify-and-jump, or minimally interactive?
	3.	Should the paper format be word-oriented or byte-oriented?
	4.	How much platform information should be embedded in the seed vs the profile sheet?
	5.	What is the best first target architecture?
	6.	What is the right first larger recovery payload after the seed?
	7.	Should the handoff contract be binary, textual, or both?

⸻

Recommended next step

Write a v1 specification for exactly one target with these sections:
	1.	ISA and platform profile
	2.	paper encoding format
	3.	seed binary layout
	4.	entry/load semantics
	5.	debugger ingress details
	6.	seed foothold contract
	7.	larger recovery payload handoff
	8.	path from recovery payload into a usable dev environment

That will force the concept into something testable.

⸻

One-sentence summary

A paper seed bootstrap is the smallest printed, architecture-specific executable foothold that can be reconstructed, injected into controllable hardware, and used to re-establish a path into larger recovery software and eventually a development environment.
