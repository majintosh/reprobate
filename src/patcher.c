#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>


int write_patch(const char* path)
{
	int fd = open(path, O_WRONLY);
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
