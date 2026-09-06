#include "patcher.h"
#include "elf_parser.h"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <stdio.h>
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

// We're going to assume our first patch that initially calls mmap is 4 bytes.
#define MMAP_CALL_SIZE 4

/* Pass addr and payload as char pointers */
#define PTRACE_WRITE(pid, addr, payload, length) \
	for (int i = 0; i<length; i++) { \
		ptrace(PTRACE_POKETEXT, pid, addr+i, payload+i); \
	}

#define PTRACE_READ(pid, addr, buffer, length) \
	for (int i = 0; i<length; i++) { \
		*(buffer+i)=ptrace(PTRACE_PEEKTEXT, pid, addr+i, buffer+i); \
	}
/*
 * argv[1] is the pid
 * argv[2] is the path to the tracee's /proc/pid/maps file (We can probably just derive this from argv[1] later)
 * argv[3] is the path to the tracee's ELF file
 * argv[4] is the name of the function we're patching
 */
int main(int argc, char* argv[])
{
	struct user_regs_struct regs;
	char buf[MMAP_CALL_SIZE];
	pid_t tracee_pid;
	struct iovec io;


	if (argc != 5) {
		printf("Provide the PID, path to the proc/maps file, ELF file, and the name of the function being patched\n");
		return -1;
	}

	tracee_pid = str_to_int(argv[1]);


	/* Attaching ptrace sends SIGSTOP; this doesn't necessarily freeze the tracee immediately */
	if (ptrace(PTRACE_ATTACH, tracee_pid, 0x0, 0x0) == -1) {
		printf("ptrace failed to attach\n");
		return -1;
	}

	waitpid(tracee_pid, NULL, WUNTRACED);

	io.iov_base = &regs;
	io.iov_len = sizeof(regs);

	ptrace(PTRACE_GETREGSET, tracee_pid, NT_PRSTATUS, &io);

	PTRACE_READ(tracee_pid, regs.rip, buf, sizeof(buf));

	return 0;
}
