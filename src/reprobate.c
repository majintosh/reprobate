#include "patcher.h"
#include "elf_parser.h"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <elf.h>

#define PATCH_FILE "/tmp/reprobate.bin"

static inline int exp(int base, int power)
{
	int b = 1;
	while (power--)
		b*=base;
	return b;
}

long get_aslr_base(const char* path)
{
	int fd = open(path, O_RDONLY);
	char base[17];
	read(fd, base, 17);
	close(fd);
	long addr = 0;
	for (int i = 0; base[i] != '-'; i++) {
		if (base[i] >= '0' && base[i] <= '9')
			addr+=base[i]-'0';
		else if (base[i] >= 'a' && base[i]<='f')
			addr+= 10 + base[i]-'a';
		addr<<=4;
	}
	addr>>=4;
	return addr;
}

int str_to_int(char* str)
{
	int i = -1; // number of digits
	while (str[++i] != '\0');
	int n = 0;
	int sum = 0;
	i--; // Subtraction here rids us of the need to always subtract i by 1 for exponentiation

	while (str[n] != '\0')
		sum+=(str[n++]-'0')*exp(10, i--);

	return sum;
}

static inline int
check_free(const pid_t pid, const int bytes, const long addr)
{
	for (int i = 0; i<bytes; i++) {
		if (ptrace(PTRACE_PEEKTEXT, pid, addr+i, NULL) != NULL)
			return 0;
	}
	return 1;
}

#define MMAP_CALL_SIZE 2

/* Pass addr and payload as char pointers */
#define PTRACE_WRITE(pid, addr, payload, length) \
	for (int i = 0; i<length; i++) { \
		ptrace(PTRACE_POKETEXT, pid, addr+i, payload[i]); \
	}

#define PTRACE_READ(pid, addr, buffer, length) \
	for (int i = 0; i<length; i++) { \
		buffer[i]=ptrace(PTRACE_PEEKTEXT, pid, addr+i, buffer+i); \
	}

#define PATCH_SIZE 32
/*
 * argv[1] is the pid
 * argv[2] is the path to the tracee's ELF file
 * argv[3] is the name of the function we're patching
 */
int main(int argc, char* argv[])
{
	char read_buf[MMAP_CALL_SIZE], write_buf[MMAP_CALL_SIZE];
	struct user_regs_struct regs_copy, regs_write;
	pid_t tracee_pid;
	struct iovec io;
	char* patch_buf;
	char maps[32];
	int size;


	if (argc != 4) {
		printf("Provide the PID, path to the proc/maps file, ELF file, and the name of the function being patched\n");
		return -1;
	}

	tracee_pid = str_to_int(argv[1]);

	snprintf(maps, sizeof(maps), "/proc/%d/maps", tracee_pid);


	/* Attaching ptrace sends SIGSTOP; this doesn't necessarily freeze the tracee immediately */
	if (ptrace(PTRACE_ATTACH, tracee_pid, 0x0, 0x0) == -1) {
		printf("ptrace failed to attach\n");
		return -1;
	}

	waitpid(tracee_pid, NULL, WUNTRACED);

	io.iov_base = &regs_copy;
	io.iov_len = sizeof(regs_copy);

	ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &io);
	PTRACE_READ(tracee_pid, regs_copy.rip, read_buf, sizeof(read_buf));

	regs_write = regs_copy;
	regs_write.rax = 9;
	regs_write.rdi = 0;
	regs_write.rsi = PATCH_SIZE;
	regs_write.rdx = PROT_EXEC | PROT_READ | PROT_WRITE;
	regs_write.r10 = MAP_PRIVATE | MAP_ANONYMOUS;
	regs_write.r8 = -1;
	regs_write.r9 = 0;

	io.iov_base = &regs_write;
	io.iov_len = sizeof(regs_write);

	/* 0x0F and 0x05 make up the syscall opcode */
	write_buf[0] = 0x0F;
	write_buf[1] = 0x05;


	/* Writing the syscall in and running it */
	PTRACE_WRITE(tracee_pid, regs_write.rip, write_buf, sizeof(write_buf));
	ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &io);
	ptrace(PTRACE_SYSCALL, tracee_pid, 0, 0);

	/* Waiting for tracee to stop at syscall entry */
	waitpid(tracee_pid, NULL, WUNTRACED);

	/* Stop tracee at syscall exit */
	ptrace(PTRACE_SYSCALL, tracee_pid, 0, 0);
	waitpid(tracee_pid, NULL, WUNTRACED);

	/* Getting the return value of mmap */
	ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &io);

	/* Restoring the initial process state */
	io.iov_base = &regs_copy;
	io.iov_len = sizeof(regs_copy);

	PTRACE_WRITE(tracee_pid, regs_copy.rip, read_buf, sizeof(read_buf));
	ptrace(PTRACE_SETREGSET, tracee_pid, NT_PRSTATUS, &io);

	size = write_patch(PATCH_FILE);
	patch_buf = malloc(size);
	if (get_patch(patch_buf, PATCH_FILE) == NULL) {
		printf("Failed to get patch\n");
		return -1;
	}

	patch_process(tracee_pid, patch_buf, size, (void*) regs_write.rax);

	/* patching the target function's prologue */
	/*
	 * 0x50 Pushes RAX
	 * 0x48 0xB8 begins our overwrite, where we move a value into the value in rax
	 * regs_write.rax is the value we want to move into rax (this is definitely wonky)
	 * 0xFF 0xE0 is the jmp binary
	 */
	char pokes[10] = {0x50, 0x48, 0xB8, regs_write.rax, 0xFF, 0xE0};
	long base = get_aslr_base(maps);
	long func = get_func_addr(argv[2], argv[3]);
	long func_addr = get_aslr_base(maps) + get_func_addr(argv[2], argv[3]);
	ptrace(PTRACE_POKETEXT, tracee_pid, func_addr, &poke);

	return 0;
}

/*
 * Patch in mmap call [X]
 * Call mmap [X]
 * Write patch into newly mapped region (process_vm_writev) [X]
 * Write trampoline to jump to new patch []
 * Write jmp call into patched function prologue to jump to trampoline (POKETEXT) [] (will a near jmp work?)
 * Write jmp call in patch to jump back to original function []
 * Restore initial state [X]
 */
