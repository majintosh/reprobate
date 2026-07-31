reprobate is an instrumentation probing thingy.

Ahem. Reprobate is a dynamic tracer that makes exclusive use of direct `jmp`s rather than `int3` breakpoints.

I'm really doing this because I wanna try making my own "prober." But, there's a more reasearchy-y aspect here, too. Well, hopefully there'll be one. I still haven't written a single line of code. Where were we? Right, the idea is to see the performance differences between `jmp`-based probes (think kprobes) and `int3`-based probes (ebpf and the like).

The main reason `int3` is even used, despite the trap overhead, is that it gives us a way to establish quiesence for a function that's being patched. I've been thinking of other workarounds, like those obscure CPU watchpoints, but they also have some latency.

I've also seen different approaches, like a stop-the-world approach, which seems quite viable. There's a big performance hit to that, sure, but it's a one-time cost that only occurs once we run the patch.

Anyway, I'm getting carried away. For now, we need to focus on building a basic "prober." Let's look at this from a higher-level.

We'll need a binary to actually patch, and we'll have to read said binary. Now, that's just *a bit* complicated. I'd much rather patch the assembly instead. Buuut, then we'd have a static prober... Or would we?

We don't really need to scan through the compiled binary. Instead, we can look through the assembly. Once we find our patching-function's prologue, we can map it to the compiled binary. Does that make sense? I hope so.

So, breaking it down into syscalls, we'll need open(), read(), and close() for the initial assembly scanning, then write() and mmap(). We need mmap() so we can map the portion of memory we place our probe in as executable.

So... I dunno where to start. Maybe we can just compile a basic hello world program. Naaah, that's too complicated. Let's just return a number for now, no need for stdio shenanigans. We can use gcc's -S flag for this.

-S isn't what we're looking for... The output doesn't have the stuff we're looking for. I see no _start function here.

Hohoho, silly me. We don't need -S, we can just use objdump to disassemble the final object file. Hmm... what's the difference between the assembly produced by -S and the one disassembled by objdump? Hmmmmmmm... I don't know... (linking, that's what)

I'm pretty sure we wanna use metadata to help us figure out where a function is. I initially thought about using seek and whatnot to search for a function we're looking for, but that seems a lot slower than just checking ELF headers, or DWARF headers. Hmm... which one to use... is that what the -g flag is? Just generates a DWARF file instead of ELF? That's what the man page says, kinda. Apparently it's all ELF; there's no such thing as a DWARF file. I'm confused.

I'm just gonna compile with -g and look at the disassembly.

So, I compiled two files. a.out was compiled without any flags, and b.out with just the -g flag. I compared the disassembly with `sdiff` and found... nothing. There is no difference between them. I guess this just means the actual assembly's the same, which makes sense. The executables themselves are different, according to sdiff.

For curiosity's sake, I compiled the same file twice with the same flags, just to see if I'd get the same compiled binary. I did, so that's nice, but I used `nvim` to add just a single character to one of them. `sdiff` then showed the two files as being different. What's strange is that when I `nvim` again to delete that char, `sdiff` *still* says they're different. Curious, curious... (I looked into this later, and found it probably has to do with silent changes brought about by editing with a text editor, like endings and stuff)

I have come to an epiphany. I've been getting lost in this whole mapping thing. So, for now, I'll set up a basic `target_addr` function that'll return a hardcoded value. I'll instead focus on the actual clobbering logic and setting up our probe.
