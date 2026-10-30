- Actually finding the funciton's address; had to write an ELF parser

- Finding the ASLR base; had to read proc/mem/maps and retrieve the first address

- Getting a region of memory: couldn't use process_vm_writev, had to instead manually inject an mmap call.

- The jmp itself! Because the allocated memory region was always too far or a near jmp, I had to load a 64-bit value into the rax register first BEFORE calling a register jmp. This meant that our patch into the function had to have a permanent `mov` instruction to load the value into the register. BUT, that's destructive, so we *also* have to push `rax` onto the stack, then pop it when we return from the trampoline.

- Boundary checking: Funnily enough, when debugging with gdb, the instrucion `(bad)` popped up a few times. I was confused, because the function I was patching was, itself, named `bad`, so I thought `(bad)` here just referred to the function; I was wrong. Turns out, `(bad)` meant that the instruction at that memory address was corrupted. That's because the patch we made actually overwrote a bit of another instruction. So, we need to make sure we only clobber whole instructions at once. To do this, we had to find the instruction boundaries, which we used the `Capstone` library for.

- RIP-relative addressing: Now, we need to figure out how to patch RIP relative stuff in our trampoline... One approach is to use memory-indirect addressing for calls and stuff, becaues just changing the displacement won't work if our memory region is mapped far away. For now, I'm going to just try and allocate the memory region closer to the text segment.

- Stack stuff: We initially pushed `rax` to the stack because we used it to jmp into the trampoline. Now, because of this, our clobbering got a lot bigger, since we need `moveabs` and `push rax` and all that. The issue here was that because the clobbering is pretty big, we also clobber instructions like `push rbp`. This causes an issue because now, the initial `rax` value we pushed is buried under that `rbp` value. Maybe we could've just popped `rax` as soon as we jumped? Hmm... Anyhoo, this links to the previous conundrum: "The jmp itself!" That conundrum was the cause of this conundrum. Conundrum.
