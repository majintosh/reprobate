#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

// Very fancy, idiomatic C right here. Just don't pass NULL and you'll be fine
static inline int str_comp(char* str1, char* str2) {
	int i = -1;
	while (str1[++i] == str2[i] && str1[i]);
	return !(str1[i] || str2[i]);
}

long get_func_addr(char* elf_path, char* func_name)
{
	int fd = open(elf_path, O_RDONLY);
	if (fd < 0)
		return -1;
	struct stat elf_stats;
	stat(elf_path, &elf_stats);
	// Bit crude in it's current state. We're mapping the entire
	// file rather than just the header parts we're interested in.
	char* base = (char*) mmap(NULL, elf_stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (base == MAP_FAILED)
		return -1;
	Elf64_Ehdr *ehdr = (Elf64_Ehdr*) base;

	Elf64_Shdr *sym_hdr = NULL;
	Elf64_Shdr *str_hdr = NULL;

	Elf64_Shdr* ptr = (Elf64_Shdr*) (base + (ehdr->e_shoff));
	for (int i = 0; (sym_hdr == NULL || str_hdr == NULL) && i<ehdr->e_shnum; i++, ptr++) {
		if (ptr->sh_type == SHT_SYMTAB)
			sym_hdr = ptr;
		else if (ptr->sh_type == SHT_STRTAB)
			str_hdr = ptr;
	}

	Elf64_Sym *symtab = (Elf64_Sym*) (base + sym_hdr->sh_offset); // should be named sym_entry or something
	for (int i = 0; i<(sym_hdr->sh_size / sym_hdr->sh_entsize); i++, symtab++) {
		if (str_comp(func_name, (base + str_hdr->sh_offset + symtab->st_name)))
			return symtab->st_value;
	}

	return -1;
}

int main(int argc, char* argv[])
{
	get_func_addr(argv[1], argv[2]);
	return 0;
}
