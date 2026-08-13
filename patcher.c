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

	// I plead idiomatic C
	return ((write(fd, buf, sizeof(buf)) == -1 | close(fd) == -1) * -1);
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

	// Very very idiomatic C
	if (read(fd, buf, patch_stat.st_size) == -1 | close(fd) == -1) {
		free(buf);
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
