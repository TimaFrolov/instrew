#include "elf-loader.h"
#include <fcntl.h>
#include <filesystem>
#include <linux/limits.h>
#include <span>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static std::span<const Elf64_Shdr>
load_elf_shdrs(const Elf64_Ehdr &elf_ex, std::span<const uint8_t> elf_data) {
  if (elf_ex.e_shentsize != sizeof(Elf64_Shdr))
    return {};

  return std::span<const Elf64_Shdr>(
      reinterpret_cast<const Elf64_Shdr *>(elf_data.data() + elf_ex.e_shoff),
      elf_ex.e_shnum);
}

static std::span<const char>
load_elf_shstrtab(const Elf64_Ehdr &elf_ex, std::span<const Elf64_Shdr> shdata,
                  std::span<const uint8_t> elf_data) {
  const Elf64_Shdr &shdr = shdata[elf_ex.e_shstrndx];

  return std::span<const char>(
      reinterpret_cast<const char *>(elf_data.data() + shdr.sh_offset),
      shdr.sh_size);
}

static std::span<const Elf64_Sym>
load_elf_symbtab(std::span<const char> shstrtab,
                 std::span<const Elf64_Shdr> shdata,
                 std::span<const uint8_t> elf_data) {
  for (const Elf64_Shdr &shdr : shdata)
    if (strcmp(shstrtab.data() + shdr.sh_name, ".symtab") == 0)
      return std::span<const Elf64_Sym>(
          reinterpret_cast<const Elf64_Sym *>(elf_data.data() + shdr.sh_offset),
          shdr.sh_size / sizeof(Elf64_Sym));

  return {};
}

static std::span<const char>
load_elf_strtab(std::span<const char> shstrtab,
                std::span<const Elf64_Shdr> shdata,
                std::span<const uint8_t> elf_data) {
  for (const Elf64_Shdr &shdr : shdata)
    if (strcmp(shstrtab.data() + shdr.sh_name, ".strtab") == 0)
      return std::span<const char>(
          reinterpret_cast<const char *>(elf_data.data() + shdr.sh_offset),
          shdr.sh_size);

  return {};
}

void free_elf_binary(ElfInfo &&info) {
  munmap(const_cast<uint8_t *>(info.base.data()), info.base.size());
}

std::variant<ElfInfo, int>
load_elf_binary(const std::filesystem::path &filename) {
  int fd = open(filename.c_str(), O_RDONLY, 0);
  if (fd < 0)
    return fd;

  const size_t elf_size = std::filesystem::file_size(filename);
  std::span<uint8_t> elf_data =
      std::span<uint8_t>(reinterpret_cast<uint8_t *>(
                             mmap(NULL, std::filesystem::file_size(filename),
                                  PROT_READ, MAP_PRIVATE, fd, 0)),
                         elf_size);
  if (elf_data.data() == MAP_FAILED)
    return -ENOMEM;

  close(fd);

  Elf64_Ehdr &elfhdr_ex = *reinterpret_cast<Elf64_Ehdr *>(elf_data.data());
  std::span<const Elf64_Shdr> shdata = load_elf_shdrs(elfhdr_ex, elf_data);
  std::span<const char> shstrtab =
      load_elf_shstrtab(elfhdr_ex, shdata, elf_data);

  return ElfInfo{
      .base = elf_data,
      .ehdr = elfhdr_ex,
      .symtab = load_elf_symbtab(shstrtab, shdata, elf_data),
      .strtab = load_elf_strtab(shstrtab, shdata, elf_data),
      .machine = elfhdr_ex.e_machine,
  };
}
