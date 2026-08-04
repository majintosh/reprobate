#include "finder.h"
#include <fcntl.h>

char* get_target_byte()
{
	return (char*) 1;
}

//void* get_aslr_base(char* path)
//{
//	int fd = open(path, O_RDONLY);	
//	char base[16];
//	read(fd, base, 16);
//	close(fd);
//}
