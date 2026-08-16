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

I have come to an epiphany. I've been getting lost in this whole mapping thing. So, for now, I'll set up a basic `target_addr` function that'll return a hardcoded value. I'll instead focus on the actual clobbering logic and setting up our probe. Placeholder functions sure are great, aren't they?

Now we need to figure out how to actually write into our probed process's address space... What sort of syscall do I need to use here? We can just look this up... OR, we can `strace` ebpf and snoop on it!

Only thing I can see here is a `bpf` syscall, which is great, I'm happy for bpf, it got its own syscall, but not quite what I'm looking for. I should just `strace` strace.

`ptrace` is what we were looking for.

Why am I doing all this snooping around when we could've just used the symbol table...

Apparently, we need to run PTRACE_ATTACH before doing anything. Why though?! Why must we first attach...? Hmmm... This sucks, I must admit; everything here's veiled in mystery. We need to stage a heist, my friends...

Here's the plan: we find *where* our tracee process's page table is, yoink it, then use it for our own, *nefarious* purposes. Muahahaha...

Why can't I ptrace... I need CAP_SYS_PTRACE, whatever that means...

No, nevermind, the issue was that the process was currently being traced by gdb. We're good now

Ok, we need to go to the lab for this part, get a pen and paper ready. Firstly, we need to figure out what to overwrite our instruction with. We want to `jmp` to `_fini`'s `ret` instruction. Why? Weell, I don't know. I don't know what `_fini` is, but I suspect it's `_start`'s counterpart. Plus, looking at the assembly, that's where `main` jumps, so, that seems like where the instruction stream eventually ends up anyway.

Looking at the intel developer's manual, we can see that there are 5 billion different `jmp` instructions. I got confused a bit, but ended up figuring out the opcode we want is FF. That's the `jmp` that performs an absolute jump rather than a relative one. Now, we *can* use a relative jump instead, but calculating the offset between our current address and the target one seems like a pain, so we'll just use FF.

Now, we need to point FF to a place where the address we want to jump to exists. This can either be another memory address, or a register. I don't understand why we can't just provide the address we wanna jump to directly instead of using a pointer. Maybe a better way to go about all this is to overwrite the value of the RIP register? I guess that's what the jmp instruction does... Anyway,

Anyway what? This is all terrible. I applaud people who have to work with assembly instructions manually, because this is agonizing. Let's just use an offset.

EB 09, that's it, that's what we've been looking for this whole time. Forget FF and far `jmp`s.

`jmp`s took me a bit to get. Rather than just calculating the offset between the current address and the target one, you also need to consider the length of the jmp instruction itself. For instance, you subtract 5 from your final offset if the `jmp` is 5 bytes.

Working with memory addresses in c is tougher than I thought... You're forced to use a long if you want to assign a value larger than 32 bits. I thought this was supposed to be a systems-first language...

Anyhoo, we've got the `jmp` stuff down, now we need to figure out a way to actually graft machine code into another process's user space. `mmap`, maybe? `copyin`? Is that even a thing?

`mmap` has a shared flag, which we can use to share some memory. But I dunno how we're supposed to get this to work. I'm sure this is great for forked processes, but what about ones that are separate from each other? Could we use a pipe, perhaps? But even pipes require forked processes... What kinda lousy IPC is this...

The answer was `process_vm_writev`, very useful function. But now, I've a dilemma of sorts... I have multiple different paths to go. I can either begin work on parsing ELF headers, so that I no longer have to hardcode function addresses, or I can get to work on generating the probe machine code. Now, I could either write my own C compiler(a subset of C, of course), or I can just rely on gcc.

The machine code generation seems a bit daunting, and I've had my fill of looking at it, so I'll just focus on ELF parsing instead.

The plan I have is as follows:
1. Open and read the Elf file...

A most wondrous idea has hit me. I was worried about having to fill out the `Elf64_Ehdr` struct and others manually, but I've been thinking about things the wrong way! I was under the false assumption I always had to call `read` with an array, but that's not truly the case. I could just pass `&ehdr` into read, and everything gets filled automatically! Wonderful!

I've been thinking... we should probably mmap the file instead of repeated `read` calls.

Wondrous idea, wondrous idea. I've been bothering with directly declaring structs and assigning by value, but my problem is that I don't like moving bytes around very much. If I only need to look at a section headers sh_type field, why should I read the entire thing? I'd thought that even if I `mmap`'d the file, I'd still have to bother with moving bytes, like a `copyin` operation.

But that's all unnecessary, because we have pointers! Declaring a pointer doesn't even do anything like move bytes, it just gives us a convenient way of grabbing memory on the go, and works really well with `mmap`.

But `mmap` requires a size_t arg, and I don't know how to get that... I wonder if I can just pass a macro, or NULL to grab the whole file... Do ELF headers have a max size? I wonder.

We can just use `stat`! Very convenient.

`sh_link` is a thing, a very useful thing that directs us to the correct string table, because there exist multiple string tables, which was the cause of some errors I had to endure. You've also gotta look out for index values and mixing them up with offsets, very troublesome things.

Now that we're able to retrieve the ELF string table info and ASLR base, we need to work on actually generating a patch. We'll just rely on gcc for this. But how?

I tried compiling with `gcc -S` and then running the output .s file through `as`, yet the issue is that `as` generates an ELF file, when all we want is just a file with straight binary. The *other* issue is that gcc generates assembly code with metadata, like .info and .ident, but I suspect we can let `as` ignore those.

Now, we *could* keep things as is, and have another ELF parser that locates the text segment, but that's too much work. I just want `as` to look at some assembly, and output a file with the corresponding machine code, nothing more, nothing less. There must be some flag that lets us do this.

We had to jump through some hoops... `as` can't output a raw binary blob, so we need to run its output through another tool, `objcopy` which just translates object files into different formats. In our case, we want raw binary, so we want to pass the `-O binary` flag.

Oh, but there's another flag—there always is. We also need to take *just* the .text section, so we also need to pass `-j .text` which stands for "just .text, please."

And now, after you run your .out file through the `objcopy -O binary -j .text a.out a.bin` machine, you get a perfectly ordinary binary blob. I verified with `stat` to check the size, as well as `hexdump`, and all looked good to me.

But throw all that out the window, because it's too elaborate. Instead, we can use `nasm` and the `-f bin` flag that does all that for us. Why doesn't `as` have the same option? Strange.

...no, no... it still doesn't work... Of course not... We can't just pass the output of `gcc -S` into `nasm`, because `gcc` likes inserting metadata that `nasm` can't ignore.

the problem we're facing here is that we're too attached to the trivialities. Who cares *how* we generate the final binary blob? All `reprobate` cares about is getting a file that contains a binary blob. So, for now, we're going to set up a placeholder "patch generator" function that writes to a new file, and the only thing it'll write is 0xc3, the `ret` instruction. The main function we'll be working on for now is one that reads that file, and runs everything through `process_vm_writev`.

It seems like the syscall requires iovecs. I'd previously encountered the things in FreeBSD, where they were necessary... No, I'm thinking about `uio`, not `iovec`.

Have you heard of the term "Idiomatic C"? It's a very nice term you can throw around whenever you're up to no good. "Hey, what're you doing there?!" "Huh. me?" "Yeah, you, what's that in your hand?" "This? This is just idiomatic C."

The most idiomatic C is the C that uses an abundance of logical operators. Really. Look at any idiomatic C and you'll just find logical operators at the root of it—the C book's filled with them. Then again, there's also stuff that doesn't use logical operators that much, like duff's device, though I don't know if that counts as idiomatic C. I don't think there's actually a meaning to the term "idiomatic C," it's just a catch-all term for tomfoolery. Maybe that was enough text to hide the fact that I forgot bitwise ANDs and ORs don't have a set evaluation order, which means my idiomatic C was buggy C this whole time.

I was concerned that compiler optimizations could inline functions, and thereby ruin my prober, since it needs the functions addresses and for them to be actual functions for ret to work. A real concern, but just a quick look at the ELF file with objdump can make clear whether or not a function was inlined.

I've got the basic prototype set up, but it doesn't seem to be working... You see, what I'm doing is running a test program that calls a function `bad` in `main`. All `bad` does is print something out. I'm trying to patch this function's prologue with `ret` so it doesn't print anything. The issue is that it's not working.

My guess right now is that either the compiler optimized out the function call and inlined it, or something with prefetching. Hmm, prefetching... I wonder how we can tell the CPU that the prefetched instructions need to be fetched again; maybe that's the point of `int3`?

Well, I can already see a different issue. The `buf` array we use to read our patch file isn't reading properly. Instead of just having the value 0xc3, it's got a completely different value. There's also the fact that the size of the buf is 8 instead of 1, for whatever reason... No, nevermind, that's just the size of the pointer itself, not the elements in it.

The *actual* issue is `process_vm_writev`'s failure. It's returning -1. Checking `errno`, we're getting an EFAULT error, which means something's wrong with how we're generating the address of the function we're trying to probe. Something about being unable to access the address... I ran through the address calculation again, though, and everything looked fine. Curiouser and curiouser

What's even more curious is that it may actually be *our* address, not the remote address. `local_iov` could also be the source of the error.

Turns out that it may be a permission issue. Looking into it, `EFAULT` can also be returned if we're trying to write to a page that doesn't have write permissions. Though that seems more like an `EPERM` error. Not very good, Linux. One point docked.

So, we need to now figure out a way to change the page permissions of a remote process. The `mprotect` man page offers no guidance on such a matter, only telling us how to change the calling process's protections.

Maybe this calls for some additional patching? `mprotect` is ultimately a syscall, which means it exists as a single assembly instruction. All we have to do is patch just one instruction (probably a few instructions, since we need to pass arguments as well) into the process we're patching that marks the text segment as writeable. Afterwards, we need to point the process's RIP to our new patch, which I don't know how to do. Maybe we could use `PTRACE_SETREGSET`? Hmm... I feel like I'm overcomplicating things, and a much simpler, more convenient solution exist somewhere. Doing all the things I just said is a lot of work...

I have cogitated and cogitated, and cogitated some more, and come to a conclusion. You see, the whole reason I opted to use `process_vm_writev` instead of `PTRACE_POKETEXT` is because `process_vm_writev` is a lot faster when it comes to writing multiple bytes from a big file. `PTRACE_POKETEXT`, on the other hand, requires a whole syscall trap for every single byte. With `process_vm_writev`, we need to write elaborate logic to change memory protections by manually hacking RIP and dealing with register saving conventoions. With `PTRACE_POKETEXT`, we *could* just use it repeatedly for every single byte of our patch, but that'd be slow. So, what do we do? We take the best of both worlds.

We can use `PTRACE_POKETEXT` to write a `jmp` instruction in the text segment, which jumps to another region of memory that's executable *and* writeable. That region is where we use `process_vm_writev` and where our patch goes. Brilliant.
