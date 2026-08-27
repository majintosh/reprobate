#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <stdlib.h>


int write_patch(const char* path)
{
	int fd = open(path, O_WRONLY);
	if (fd == -1)
		return -1;
	char buf[1] = {0xc3};

	int writes = write(fd, buf, sizeof(buf));
	int closed = close(fd);
	return (writes == -1 || closed == -1);
}

char* get_patch(const char* path)
{
	struct stat patch_stat;
	if (stat(path, &patch_stat) == -1)
		return NULL;
	char* buf = (char*) malloc(patch_stat.st_size);
	if (buf == NULL)
		return NULL;
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		free(buf);
		return NULL;
	}

	int reads = read(fd, buf, patch_stat.st_size);
	int closed = close(fd);
	if (reads == -1 || closed == -1) {
		free(buf);
		return NULL;
	}

	return buf;
}

long
get_stack_end(const pid_t pid)
{
	/* placeholder */
	return 0x1;
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

int
patch_stack_end(const pid_t pid, const char* patch, const size_t len)
{
	long addr;

	addr = get_stack_end(pid) - len;

	return (patch_process(pid, patch, len, addr));
}
