#include "finder.h"
#include <sys/ptrace.h>
#include <sys/types.h>
#include <stdio.h>

static inline int exp(int base, int power)
{
	int b = 1;
	while (power--)
		b*=base;
	return b;
}

struct jmp {
	char bytes[5];
};
// Returns the desired JMP binary
static struct jmp jmp_generator(char* current_addr, char* target_addr)
{
	struct jmp out;
	out.bytes[0] = 0xE9;
	long offset = target_addr-current_addr;
	offset-=4;
	for (int i = 1; i<5; i++) {
		out.bytes[i] = (int)offset&0xFF;
		offset>>=8;
	}

	return out;
}
int main(int argc, char* argv[])
{
	int i = -1; // number of digits
	while (argv[1][++i] != '\0');
	int n = 0;
	int sum = 0;
	i--; // Subtraction here rids us of the need to always subtract i by 1 for exponentiation

	while (argv[1][n] != '\0')
		sum+=(argv[1][n++]-'0')*exp(10, i--);

	pid_t tracee_pid = sum;

	ptrace(PTRACE_ATTACH, tracee_pid, 0x0, 0x0);
	
	char* target_addr = (char*) 0x5640f258f12d;
	char* final_addr = (char*) 0x5640f258f138;
	struct jmp opcode = jmp_generator(target_addr, final_addr);

	long data = 0x0;

	// Weird endianness stuff happening
	for (int i = 4; i>=1; i--) {
		data+=opcode.bytes[i];
		data<<=8;
	}
	data+=opcode.bytes[0];

	ptrace(PTRACE_POKEDATA, tracee_pid, target_addr, data);
	return 0;
}
