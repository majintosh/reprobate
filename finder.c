#include <stdio.h>
#include "finder.h"
#include <unistd.h>
#include <fcntl.h>

char* get_target_byte()
{
	return (char*) 1;
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

int main()
{
	printf("%lx\n", get_aslr_base("/proc/1/maps"));
	return 0;
}
