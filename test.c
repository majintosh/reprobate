#include <stdio.h>
#include <unistd.h>

int flag = 0;

int bad()
{
	printf("REPROBATE FAILED\n");
	flag = 1;
	return 1;
}

int main()
{
	sleep(400);
	bad();
	return 0;
}
