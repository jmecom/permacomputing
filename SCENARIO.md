# Scenario

## Purpose

This document describes the concrete failure-and-recovery scenario that the paper seed bootstrap is meant to help with.

The point is not "how do I boot a perfect system from nothing."

The point is:

how do I recover a path back into software on a target machine when the normal development environment is gone

## Starting conditions

You have a board, SoC, or machine that is at least partially controllable.

You still have some ingress path such as:

- JTAG or SWD
- a ROM downloader
- a flash programmer
- a monitor built into the chip
- any path that can write memory and start execution

You do **not** have a usable target-side development environment.

That may mean you do not have:

- a compiler for the target
- an assembler for the target
- an SDK
- vendor flashing tools
- trusted firmware already installed
- working storage on the target
- network access
- package repositories
- a host setup capable of rebuilding the first useful target-side tool from source

## What you do have

You previously preserved a tiny first executable for that target family.

That preserved seed may exist as:

- printed machine-code bytes on paper
- redundant paper copies
- durable offline media
- a previously built binary artifact

The key point is that the first useful target-side executable was built ahead of time and preserved ahead of time.

You are not trying to compile it now under degraded conditions.

## The recovery path

The intended flow is:

1. Identify the target architecture/profile.
2. Reconstruct the seed bytes from paper or other preserved media.
3. Inject the seed into RAM or flash through the available ingress path.
4. Start executing the seed.
5. Use the seed as the first reliable target-side foothold.
6. From that foothold, bring up the next layer.
7. Use the next layer to move toward a usable development environment.

## What the seed is for

The seed is not the final dev environment.

The seed is the first preserved executable foothold that turns:

- "I can write bytes and set PC"

into:

- "I now have a repeatable target-side control contract"

Depending on the implementation, that foothold might be:

- a tiny verify-and-jump stub
- a tiny stack-language monitor
- a tiny Forth-like runtime
- a minimal set of primitives for memory inspection, patching, loading, and jumping

The seed only needs to do enough to make the **next** preserved payload reliable to enter.

## What happens after the seed

Once the seed is running, you use it to bootstrap the next layer.

That next layer may provide:

- UART bring-up
- MMIO patching
- flash writing
- a larger loader
- a more comfortable monitor
- storage access
- the first pieces of an eventual development environment

This is where the real climb begins.

The seed exists to make this next step possible without needing to compile the first target-side program on the spot.

## Why this matters

Without a preserved seed, the operator may be stuck in a bad loop:

- the target needs software to become useful
- but building that software requires a working toolchain
- and restoring the toolchain may itself depend on already having working software on the target

The seed breaks that loop by moving the first useful target-side executable into the set of things that were preserved in advance.

## Example situation

Imagine:

- you have an ARM microcontroller board
- the board still responds to SWD
- you can halt the CPU and write SRAM
- but you do not have the original vendor SDK, build system, or flashing workflow
- you do not want to rely on rebuilding a whole firmware image from source just to regain basic control

In that situation, a preserved paper seed helps because you can:

1. reconstruct the known-good seed
2. write it directly into memory
3. execute it
4. use it to establish the next recovery layer

That is enough to begin climbing back toward a real development setup.

## Non-goal

This project is not trying to guarantee recovery for every board under every condition.

It is trying to make a specific class of recovery practical:

- hardware is still at least partially controllable
- some ingress path still exists
- the first useful executable was preserved beforehand

If those are true, the machine may still be reclaimable.
