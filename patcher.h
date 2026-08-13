#include <sys/types.h>

int write_patch(const char* path);
char* get_patch(const char* path);
int patch_process(const pid_t pid, const void* buf, const size_t len, const void* target_address);
