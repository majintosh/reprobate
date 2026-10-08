Existing tracers, like `ftrace` primarily use `int3` breakpoints for arbitrary binaries. `ftrace` *does* use `jmp`-based patching, but only for kernel functions that have `NOP` buffers at the beginning of function prologues. But if you wanted to actually patch an arbitrary userspace function, or a kernel module you load in (.ko file) you'd resort to `int3`.

The issue with `int3` is that its costly. So, why don't we just use `jmp`? Because of quiescence.

The only reason we use `int3` is because it provides a way for us to maintain quiescence. With it, we're able to make sure that no thread is currently executing an instruction that we're about to patch. It provides a safety net. Saving yourself from a crash/panic is worth a lot more than performance (most of the time).

So, what can we do? One thing we could do is a stop-the-world approach. We halt all threads, make sure they're not running the instruction we're about to patch, and then proceed to patch it. This costs a *lot*, but it may save us down the road if we don't have to pay as much later.

My current "hypothesis" is that `jmp`-based patching probably makes a difference with high-frequency functions. For instance, a function called 1k times a second is going to have to pay the `int3` tax 1000 times in a single second, and the tax, however negligible it may be individually, could add up.

This probably makes a bigger difference percent-wise for smaller functions. If we have a really small function that takes, lets say, 1 millisecond, and the `int3` tax costs 0.1 milliseconds, it's not a lot, but relative to the function itself, that's a 10% difference.

Additionally, `int3` kinda wrecks CPU optimizations, like branch predicitions and prefetching. In contrast, `jmp`s don't conflict with them, so there's that performance cost to also consider.

Maybe what we need to look for in specific is a formula? A formula that says "if your function runs X times, then `jmp`-based patching is better."

Maybe we can guarantee quiescence by patching an instruction that's guaranteed to be in a function prologue? You know how every function prologue has the `lea` instruction? Maybe we could patch that? The reason I say this is because our previous issue was that we couldn't really patch atomically. As in, we could be writing a `jmp` instruction that's 5 bytes into a bigger instruction that's 8 bytes. Maybe a thread was executing the 4th byte of that 8 byte instruction, and moved on to the 5th, only to find the last byte of the `jmp` instruction. We don't need to worry about this uncertainty if we know the format of function prologues, since they'll always adhere to a format we know.

It'd be useful if we could just define custom instructions. Like an FPGA. You know how you have hardware breakpoint registers? They seem like convenient features added, like toppings to a meal. Why not do that for custom instructions? Paradoxically, the people interested enough to use it probably care about performance, and would opt for other avenues once they realize a custom instruction is slower than pre-defined ones. In the end, you're left with a forlorn instruction, shunned by its creators. Tragic...
