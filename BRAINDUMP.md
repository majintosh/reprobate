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

Reeeewiiiind. Rewind. Let's actually figure out why `process_vm_writev` sets errno to EFAULT from first principles. Specifically, let's figure things out through tracing. Come hither, `bpftrace`.

We need to first figure out where errno is stored as a variable and have some sort of hardware breakpoint to watch when the value changes, like what x64dbg provides. We *only* watch this value once `process_vm_writev` is entered. My hope is that we can catch the exact value of RIP when that value is set. With that, we get the memory address, which we then run through `nm` and `addr2line` to figure out which line of code in which file we should look at. Let's look at the logic of this whole thing ourselves.

So, my `bpftrace` script isn't working, and I think it's because `gdb`'s also attached to the process, so maybe it's got a hold on the debug registers and blocks `bpftrace`? I need `gdb` because that's how I even get the address of `main` in the first place.  I can't really just attach `gdb` for a bit then detach, because once I detach, the program's just going to run to completion. Maybe I should add a `sleep` call?

Why am I trying to use `bpftrace` you may ask? I don't know, to be honest. We can just use `gdb`; it's got functions like watch, rwatch, and awatch, which set up hardware breakpoints.

Turns out we didn't even need `nm` and `addr2line`, because `gdb` tells us which file the current line of code's at. In our case, it's "../sysdeps/unix/sysv/linux/process_vm_writev.c:30" 

This file tells me nothing, really. I see a function `process_vm_writev` which just returns this macro:

```c
return INLINE_SYSCALL_CALL (process_vm_writev, pid, local_iov,
			      liovcnt, remote_iov, riovcnt, flags);
```

Do you know what this macro expands to? This.

```c
({
  long int sc_ret = ({ unsigned long int resultvar; __typeof__ (((__typeof__ ((flags) - (flags))) (flags))) __arg6 = ((__typeof__ ((flags) - (flags))) (flags)); __typeof__ (((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt))) __arg5 = ((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt)); __typeof__ (((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov))) __arg4 = ((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov)); __typeof__ (((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt))) __arg3 = ((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt)); __typeof__ (((__typeof__ ((local_iov) - (local_iov))) (local_iov))) __arg2 = ((__typeof__ ((local_iov) - (local_iov))) (local_iov)); __typeof__ (((__typeof__ ((pid) - (pid))) (pid))) __arg1 = ((__typeof__ ((pid) - (pid))) (pid)); register __typeof__ (((__typeof__ ((flags) - (flags))) (flags))) _a6 asm (""r9"") = __arg6; register __typeof__ (((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt))) _a5 asm (""r8"") = __arg5; register __typeof__ (((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov))) _a4 asm (""r10"") = __arg4; register __typeof__ (((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt))) _a3 asm (""rdx"") = __arg3; register __typeof__ (((__typeof__ ((local_iov) - (local_iov))) (local_iov))) _a2 asm (""rsi"") = __arg2; register __typeof__ (((__typeof__ ((pid) - (pid))) (pid))) _a1 asm (""rdi"") = __arg1; asm volatile ( ""syscall\n\t"" : ""=a"" (resultvar) : ""0"" (311), ""r"" (_a1), ""r"" (_a2), ""r"" (_a3), ""r"" (_a4), ""r"" (_a5), ""r"" (_a6) : ""memory"", ""cc"", ""r11"", ""cx""); (long int) resultvar; });
  __builtin_expect ((((unsigned long int) (sc_ret) > -4096UL)), 0) ? ({ (__libc_errno = ((-(sc_ret)))); -1L; }) : sc_ret;
}
)
```

It's scary. I've sprinked in some newlines to make it a bit more readable.

```c
({
  long int sc_ret = ({ unsigned long int resultvar;
  __typeof__ (((__typeof__ ((flags) - (flags))) (flags))) __arg6 = ((__typeof__ ((flags) - (flags))) (flags));
  __typeof__ (((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt))) __arg5 = ((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt));
  __typeof__ (((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov))) __arg4 = ((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov));
  __typeof__ (((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt))) __arg3 = ((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt));
  __typeof__ (((__typeof__ ((local_iov) - (local_iov))) (local_iov))) __arg2 = ((__typeof__ ((local_iov) - (local_iov))) (local_iov));
  __typeof__ (((__typeof__ ((pid) - (pid))) (pid))) __arg1 = ((__typeof__ ((pid) - (pid))) (pid));
  register __typeof__ (((__typeof__ ((flags) - (flags))) (flags))) _a6 asm (""r9"") = __arg6;
  register __typeof__ (((__typeof__ ((riovcnt) - (riovcnt))) (riovcnt))) _a5 asm (""r8"") = __arg5;
  register __typeof__ (((__typeof__ ((remote_iov) - (remote_iov))) (remote_iov))) _a4 asm (""r10"") = __arg4;
  register __typeof__ (((__typeof__ ((liovcnt) - (liovcnt))) (liovcnt))) _a3 asm (""rdx"") = __arg3;
  register __typeof__ (((__typeof__ ((local_iov) - (local_iov))) (local_iov))) _a2 asm (""rsi"") = __arg2;
  register __typeof__ (((__typeof__ ((pid) - (pid))) (pid))) _a1 asm (""rdi"") = __arg1;
  asm volatile ( ""syscall\n\t"" : ""=a"" (resultvar) : ""0"" (311), ""r"" (_a1), ""r"" (_a2), ""r"" (_a3), ""r"" (_a4), ""r"" (_a5), ""r"" (_a6) : ""memory"", ""cc"", ""r11"", ""cx"");
  (long int) resultvar;
  });

  __builtin_expect ((((unsigned long int) (sc_ret) > -4096UL)), 0) ? ({ (__libc_errno = ((-(sc_ret))));
  -1L;
  }) : sc_ret;

}
)
```

From what little I could get. a lot of this is just assigning the value of sc_ret in a fancy manner. We first define resultvar as an unsigned long int, then use it to store the return value of the syscall. Afterwards, that final line, `(long int) resultvar;` seems out of place, but it's just what we return to the assignment operator of `sc_ret`.

In that assigment of sc_ret, we call the syscall, which you can spot in the final `asm volatile` line.

What I find to be the most interesting is the regular use of `register`. That's got a reputation for being *the* vestigial organ of C, since compilers mostly ignore it. I guess not in this case. Very interesting stuff here, though. I notice the snippets like `asm (""r9"")` are probably used to reference a register. Here I thought asm was just keyword like `static` or `volatile`, but it seems to be something we can use wherever.

Anyway, all this hasn't led us anywhere. I was expecting a very nice looking bit of code that says "we're returning `EFAULT` over here!" Instead, we got what you just saw. Let's go back to the project and lay this particular tangent to rest for now. As they say, let sleeping hogs sleep, or something.

So, `jmp`. We need to patch a basic `jmp` instruction into the process with `PTRACE_POKETEXT` and jump to a region we've patched out. But how exactly do we select a region of memory for this? We can't just use any old address, since it could be used by the process at the moment, or acquired later on by some memory allocator. What we need is a way to remotely call `mmap` on a separate process, but that's not possible. We'd probably need to patch in an `mmap` call first, but the whole issue is that we can't just patch things in all willy-nilly without acquiring memory that's safe to use. What do you think? I'm sure you have some idea, don't you? Think very hard, and pass that message to me through time.

I think one approach that might work is for us to actually use our tracer's process address space as temporary storage. What I mean by this is that we can use something like `process_vm_readv` to read the data in a certain memory range. Afterwards, we can use `process_vm_writev` to overwrite the data there—no, no, nevermind, that's too destructive...

How's this for an idea: we first use `PTRACE_PEEKTEXT` to look at some memory in the other process and store it in our own process, then `PTRACE_POKETEXT` to patch in an `mmap` call. Once the call is done, we can use `PTRACE_POKETEXT` again to restore the data that we'd overwritten.

I need to figure out a plan of action. This is it. We first call `process_vm_readv` on some writeable and executable region of memory, save it, and overwrite it with `process_vm_writev` where we patch in an `mmap` call. We set RIP to the new address, wait for the call to finish, then recover the memory we just overwrote. Once that's done, we write our patch to that new region of memory we just mapped, then finally use `PTRACE_POKETEXT` to write the `jmp` in the text segment.

I think the region we'll use is the stack. Specifically, the very bottom of the stack. If my computer science fundamentals are correct, this should be fine, as the stack grows downwards. Ergo, we'll likely be using memory that isn't currently in use. And if it is in use? Well, we were probably going to overflow the stack anyway.

I've been trying to figure out a way to get the process's stack address, and realized that it'll be an arduous process. I could read the `/proc/pid/maps` file again, but that approach seems byzantine, since we'd have to go back into string matching and keeping track of previouly read values and... Let's just use a tried and true placeholder function.

Now I'm facing a different problem, how do I retrieve the value of the `mmap` call? An elaborate IPC setup probably isn't the way to go... or is it?

We might be able to map a shared file into the tracee's process space, a file our tracer already has mapped into its own address space. Alternatively, we could just check the `/proc/pid/maps` file for new entries.

I have figured it out! Well, I figured out a previous problem, not the current one. Snooping around the `proc` filesystem and its manpages, I came across `/proc/pid/stat`, which, according to its manpage, contains a value `startstack` which returns the beginning (i.e., bottom) of the stack, which is what we were looking for! I hope I got that right... We might actually be looking for the value that's on the other end, the "top". We'll figure it out as we go. There's also `arg_start`, which says it refers to the "program environment." I don't know what that is. `env`, maybe? I dunno, but it could be the way we figure out the ASLR base, too.

The `proc` filesystem has lots of tools we can work with. `/proc/pid/mem` lets us treat the entire process's memory space as a file with things like `open()` and `read()`, though no `write()`...

`/proc/pid/attr` is just a bunch of security stuff... I must admit, keeping all your security attributes in one place seems smelly.

Looking at `get_patch`, I realize it's probably better if we pass a pre-allocated buffer rather than `malloc`'ing one in the function itself. Ownership gets fuzzy when we do stuff like that.

All this diving into different filesystems makes me wonder how exactly they implement their own read/open/write calls. Is that how it works? I don't know.

We have a problem of sorts. The stack doesn't have executable permissions.

Back to square one with us! Wondeful.

We should use ptrace.

This thing has been the death of me. I just don't know where to put in my `mmap` call. Confound it. I'm just going to set up a real basic function. We'll use `PTRACE_PEEKTEXT` to scan for a region of memory that's free. It's not very good, performance-wise, but we'll deal with it later.

That wasn't it either. I've landed at a new solution: we'll just write our `mmap` call into whatever `rip` is currently pointing at. Once we're done, we can recover the previous state.

There's currently an issue with how we flip-flop between char* and long to represent addresses. I can't use void* (though I would've liked to) since it doesn't support pointer arithmetic without extensions, so we're forced to choose between either char* or long. I'll just use char* from now on, since I don't know how... nevermind, I forgot we can't perform bit-shifts on pointers. Maybe we should just typedef a long.

I've found the most interesting thing: rip is pointing to a shared library text segment. I didn't consider this, to be honest, but I guess it makes sense.

I figured out how to read the right place, and verified that rip does indeed point to a proper memory address; now, I need to figure out how to write an mmap call.

`mmap` is a syscall, so we *should* be able to just patch in a `syscall` instruction.

I have discovered the scripting of gdb. I was wondering if the libraries I was using were using the legacy `int` instruction, or the newer `syscall` instruction. I wasn't sure, so I looked into gdb's scripting, and came up with this bad boy:

```
while ( ( (char*)$rip )[0] != 0x0F || ( (char*)$rip )[1] != 0x05)
    stepi
    end
end
```

Don't mind all the parenthesis, that's just me being unsure of precedence, as always. I wonder if we can pass arguments to a script, instead of having to manually write the bytes we're looking for.

Anyway, the intention here is to find the `syscall` instruction. Endianness always gets me, since I'm not sure how things are ordered. The intel manual says syscall's opcode is `0F 05`, but how's that actually stored? I've since elucidated that endianness doesn't apply to opcodes; those are always stored in "order."

Blegh, explaining this is confusing. It's easier to think of it in memory order. `0x0F` will always be stored at the lower memory address, and `0x05` at the higher one. That's how it works for all opcodes. Thank you, rubber duck.

Also, gdb scripts *do* take args. You just use `$arg0`, `$arg1`, etc. So, we now have this new and improved script:

```
while ( ((char*)$rip)[0]!=$arg0 || ((char*)$rip)[1]!=$arg1 )
    stepi
    end
end
```

From what I've found, the number of the syscall is 9.

I spent so long trying to figure out why `mmap` was returning a negative number to rax. Turns out, I forgot to add the `MAP_PRIVATE` flag. Should read the manpage more carefully next time.

Anyway, we finally managed to do it! We freeze the process, write an mmap call in, get the value of rax, which gives us the memory address of the new map, then restore the initial state. Immediately, I can see that this is going to have issues in multi-threaded contexts, since another thread, blitzing through the text segment, as it should, might have a head-on collision with our `mmap` call. For now though, we'll just use this.

I've been trying to use `process_vm_writev` to write into our newly allocated region, and it all seems to be going well, judging by its return value. Yet, when I check the memory of the tracee with `gdb`, I don't see my patch anywhere...

Yes, the issue was that I had assumed my `write_patch` function was returning the size of the patch in bytes when it was actually returning a boolean value. Then there was also the issue of me passing the process's initial `rip` instead of `rax` (rax contains the mmap address). Silly mistakes, don't mind me.

Anyway, it finally works. We are able to patch a running process. Hooray! Ten cheers, ten cheers.

Now I'm thinking of whether or not we should be using a `jmp` or `call` instruction. Given that this whole thing's about performance, I'll just go with plain old `jmp`s for now.
