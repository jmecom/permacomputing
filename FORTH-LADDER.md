# Forth Ladder

This document defines how much Forth-like capability belongs in the paper-preserved seed, and how the rest of the bootstrap ladder should be grown after the seed is already running.

## Core rule

The seed should contain the smallest mechanism for growing Forth, not a large Forth.

The paper-preserved seed must stay small enough to remain realistic as a printed recovery artifact. So the seed should only include the features that are necessary to establish a live target-side foothold and let the developer begin extending it.

## Seed boundary

The seed should keep only:

- debugger mailbox input and output
- a tiny token interpreter
- a data stack
- memory primitives such as `@`, `!`, `C@`, `C!`, and `D`
- control-transfer primitives such as `J` and `B`
- one minimal extension mechanism

The seed should not try to include:

- a large dictionary
- lots of core words
- board-specific drivers
- flash tooling
- storage abstractions
- a large monitor
- a full development environment

## Extension strategy

The next feature added to the seed should be exactly one self-extension mechanism.

The best candidate is:

- execute a token stream from RAM

That gives the seed a way to run larger scripts or command streams without manually retyping every command through the debugger mailbox. It is a small step in implementation size, but a large step in practical Forth-ness.

If that proves insufficient, the next feature after that can be:

- a tiny named-word facility

Only after that should we consider more traditional Forth features such as `:` and `;`.

## Developer-loaded Forth layer

The richer Forth environment should not live in the paper-preserved seed.

Instead, once the seed is alive, the developer should load a larger Forth layer that provides:

- reusable definitions
- more core words
- a richer dictionary
- better operator ergonomics
- board bring-up helpers
- loaders for larger payloads

This second layer is where the system becomes comfortable. The seed only exists to make loading that layer possible.

## Ladder shape

The intended bootstrap ladder is:

1. Start the generic seed through debugger mailbox I/O.
2. Use the built-in primitives to inspect and patch hardware.
3. Use the seed's minimal extension mechanism to run larger token streams.
4. Load a richer Forth layer into RAM.
5. Use that richer layer to bring up better capabilities such as UART, flash write, storage, or a loader.
6. Use those capabilities to load the next substantial payload.
7. Transfer control to the next layer.

## Testing strategy

The tests should exercise the ladder, not just the seed in isolation.

The first end-to-end bootstrap test should look like:

1. Start the seed.
2. Hand-enter a few mailbox commands.
3. Use those commands to load and execute a small RAM token stream.
4. Load a richer Forth layer.
5. Use that layer to load or jump to the next payload.

That test would validate the actual project idea:

- a tiny preserved seed
- first growth through a Forth-like environment
- larger capabilities loaded later

## Deferred features

These are explicitly later-layer concerns unless testing proves otherwise:

- flash write support
- storage support
- UART bring-up
- richer loaders
- a larger resident environment

Those may become important, but they should not bloat the seed unless we can demonstrate that the ladder fails without them.
