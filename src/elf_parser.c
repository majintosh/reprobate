#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>

// Very fancy, idiomatic C right here. Just don't pass NULL and you'll be fine
static inline int str_comp(const char* str1, const char* str2) {
	int i = -1;
	while (str1[++i] == str2[i] && str1[i]);
	return !(str1[i] || str2[i]);
}

long get_func_addr(const char* elf_path, const char* func_name)
{
	int fd = open(elf_path, O_RDONLY);
	if (fd < 0)
		return -1;
	struct stat elf_stats;
	stat(elf_path, &elf_stats);
	// Bit crude in it's current state. We're mapping the entire
	// file rather than just the header parts we're interested in.
	const char* base = (char*) mmap(NULL, elf_stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (base == MAP_FAILED)
		return -1;

	Elf64_Ehdr *ehdr = (Elf64_Ehdr*) base;

	Elf64_Shdr *sym_hdr = (Elf64_Shdr*) (base + (ehdr->e_shoff));

	for (int i = 0; sym_hdr->sh_type != SHT_SYMTAB && i<ehdr->e_shnum; i++, sym_hdr = (Elf64_Shdr*) (base + (ehdr->e_shoff) + i*(ehdr->e_shentsize)));

	Elf64_Shdr *str_hdr = (Elf64_Shdr*) (base + ehdr->e_shoff + sym_hdr->sh_link*(ehdr->e_shentsize));

	for (int i = 0; i<(sym_hdr->sh_size / sym_hdr->sh_entsize); i++) {
		Elf64_Sym *symtab = (Elf64_Sym*) (base + sym_hdr->sh_offset + i*(sym_hdr->sh_entsize));
		if (str_comp(func_name, (base + str_hdr->sh_offset + symtab->st_name)))
			return symtab->st_value;
	}

	return -1;
}
