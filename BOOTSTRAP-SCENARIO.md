# Bootstrap Scenario

You load the seed into RAM with JTAG, SWD, or whatever ingress path still works. When it starts, it does not try to boot a full OS. It becomes a tiny Forth-like REPL that you drive through debugger mailbox I/O.

At first, you do not assume UART, storage, networking, or flash tooling. The debugger is the transport. The seed exposes an input buffer, an output buffer, and a few control words in RAM. You write a command into the input buffer, resume execution, then read the result back from the output buffer. That gives you a crude but live target-side programming environment even when no normal console exists.

The first goal is not "boot the OS." The first goal is "gain one better capability." Usually that means one of:

- working UART
- working flash write
- working storage read
- a hex or text loader

The path looks like this:

1. Inspect and patch hardware.
Use words like `@`, `!`, `C@`, `C!`, and `D` to read and write MMIO registers until clocks, pinmux, reset bits, or a serial block start behaving.

2. Wrap that in words.
Once you discover the right register sequence, define small words for it inside the Forth environment: `UART-TX`, `UART-RX`, `TXRDY?`, `EMIT`, `KEY`, or whatever the machine needs.

3. Build a loader inside Forth.
Hand-enter a tiny loader that can accept hex or text records and write them into RAM or flash.

4. Use that loader to enter larger code.
Once the loader works, you stop doing raw memory pokes for everything. You stream in a larger monitor, better drivers, an assembler, a loader, or an OS image.

5. Jump into the next layer.
When the larger payload is in memory and verified enough, use `J`, `B`, or the equivalent handoff word to transfer control.

So the seed gets you to the next stage by letting you manually bootstrap one real transport or storage path, then use that path to load something larger.

## Why Forth

The point of using Forth here is not nostalgia. It is that you cannot assume a compiler toolchain exists yet.

You use a Forth-like seed because:

- you already preserved it as executable bytes
- it is interactive
- it lets you define new behavior on the target itself
- it does not require an assembler, compiler, linker, or SDK before you can start programming the machine

Instead of needing a complete development environment first, you get a tiny live system that still lets you extend the target from the inside.

That is why Forth is attractive for this project. It is a survival language: small, interactive, extensible, and useful before the normal software stack exists.
