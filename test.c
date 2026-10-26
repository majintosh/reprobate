#include <stdio.h>
#include <unistd.h>

int flag = 0;

int bado()
{
	printf("REPROBATE FAILED\n");
	flag = 1;
	return 1;
}

int main()
{
	sleep(100);
	bado();
	return 0;
}
