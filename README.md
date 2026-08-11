# reprobate

A dynamic instrumentation tool that patches functions using direct `jmp` instructions instead of `int3` breakpoints. The goal is to compare the overhead of `jmp`-based patching against `int3`-based tracers like eBPF, which pay a trap cost on every hit.

## Status: in progress

**Done:**
- ELF symbol-table parsing (mmap-based section header and symtab/strtab traversal) to resolve function addresses without hardcoding
- ASLR base-address calculation for a running process

**Not yet done:**
- Generating patch payloads (compiling a target snippet via GCC and preparing it for injection)
- Writing patches into a running process via `ptrace` / `process_vm_writev`
- Benchmarking `jmp` vs `int3` overhead

## How it works (planned)

1. Parse the target binary's ELF headers to locate a function's address, accounting for ASLR
2. Compile a small C snippet with GCC to generate the probe's machine code
3. Attach to the target process with `ptrace` and write the patch via `process_vm_writev`
4. Compare runtime overhead against an `int3`-based approach

See [BRAINDUMP.md](./BRAINDUMP.md) for the dev log
