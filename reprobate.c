#include "patcher.h"
#include "elf_parser.h"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

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

/*
 * argv[1] is the pid
 * argv[2] is the path to the tracee's /proc/pid/maps file (We can probably just derive this from argv[1] later)
 * argv[3] is the path to the tracee's ELF file
 * argv[4] is the name of the function we're patching
 */
int main(int argc, char* argv[])
{

	pid_t tracee_pid = str_to_int(argv[1]);

	ptrace(PTRACE_ATTACH, tracee_pid, 0x0, 0x0);
	
	long aslr_base = get_aslr_base(argv[2]);
	long function_address = aslr_base + get_func_addr(argv[3], argv[4]);

	write_patch(PATCH_FILE);
	char* buf = get_patch(PATCH_FILE);
	if (buf == NULL)
		return -1;
	// The 1 here is just a placeholder, we need to actually figure out how many bytes we're moving later (maybe just use write_patch's return value)
	patch_process(tracee_pid, buf, 1, (void*) function_address);

	return 0;
}
