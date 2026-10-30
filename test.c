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
	sleep(2);
	bado();
	printf("FINISHED\n");
	return 0;
}
