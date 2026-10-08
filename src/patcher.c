#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>


int write_patch(const char* path)
{
	int fd = open(path, O_WRONLY | O_CREAT); // Remove the creat flag later, bad practice
	if (fd == -1)
		return -1;
	char buf[5] = {0xc3, 0xff, 0x12, 0xca, 0xde};

	int writes = write(fd, buf, sizeof(buf));
	int closed = close(fd);
	return writes;
}

char* get_patch(const char* buf, const char* path)
{
	struct stat patch_stat;
	int fd;
	if (stat(path, &patch_stat) == -1)
		return NULL;
	if (buf == NULL)
		return NULL;
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		return NULL;
	}

	int reads = read(fd, buf, patch_stat.st_size);
	int closed = close(fd);
	if (reads == -1 || closed == -1) {
		return NULL;
	}

	return buf;
}

ssize_t patch_process(const pid_t pid, void* buf, const size_t len, void* target_address)
{
	struct iovec local, remote;
	local.iov_base = buf;
	local.iov_len = len;
	remote.iov_base = target_address;
	remote.iov_len = len;
	return process_vm_writev(pid, &local, 1, &remote, 1, 0);
}

/* target_addr takes an address to an address 
 * We're using the FF jmp that takes a 64-bit offset
 * */
//char* jmp_generator(const char* buf)
//{
//	buf[0] = 0x50; // Pushes RAX to the stack
//
//	/* jmp encoding */
//	buf[1] = 0xFF;
//	buf[2] = 0xE0; // The ModRM byte. Points to rax reg	
//	return buf;
//}
//
///* This patches in a jump */
//char* patch_jump()
//{
//	/* First thing to do is push the current RAX to the stack, then overwrite it with our trampoline mem address */
//	/* The trampoline should include a pop instruction to restore the RAX reg before we return to the original function */
//}
