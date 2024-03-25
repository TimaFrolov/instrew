#pragma once

#include <elf.h>
#include <filesystem>
#include <span>
#include <variant>

struct ElfInfo {
  std::span<const uint8_t> base;
  Elf64_Ehdr &ehdr;
  std::span<const Elf64_Sym> symtab;
  std::span<const char> strtab;
  Elf64_Half machine;
};

std::variant<ElfInfo, int>
load_elf_binary(const std::filesystem::path &filename);

void free_elf_binary(ElfInfo &&info);
