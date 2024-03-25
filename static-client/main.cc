#include "elf-loader.h"
#include "translator.h"
#include <cstdio>
#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <llvm/Support/SourceMgr.h>
#include <variant>

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cout << "usage: CONFSTR EXECUTABLE [ARGS...]";
    return 1;
  }

  char *server_config = argv[1];
  argc -= 2;
  argv += 2;

  auto _info = load_elf_binary(argv[0]);
  if (std::holds_alternative<int>(_info)) {
    puts("error: could not load file");
    return std::get<int>(_info);
  }
  ElfInfo info = std::get<ElfInfo>(_info);

  Translator translator;
  translator.elf = info.base;
  TranslatorServerConfig tsc;
  tsc.tsc_guest_arch = info.machine;
#ifdef __x86_64__
  tsc.tsc_host_arch = EM_X86_64;
  tsc.tsc_stack_alignment = 8;
#elif defined(__aarch64__)
  config.tsc_host_arch = EM_AARCH64;
#else
#error "Unsupported architecture!"
#endif
  int init_retval = translator.init(server_config, tsc);
  if (init_retval) {
    puts("error: could not init translator");
    return init_retval;
  }

  auto _tc = translator.config_fetch();
  if (std::holds_alternative<int>(_tc)) {
    puts("error: could not fetch config");
    return std::get<int>(_info);
  }
  TranslatorConfig tc = std::get<TranslatorConfig>(_tc);

  auto _initobj = translator.get_object();
  if (std::holds_alternative<int>(_initobj)) {
    puts("error: could not fetch initial object");
    return std::get<int>(_initobj);
  }
  std::vector<uint8_t> initobj = std::get<std::vector<uint8_t>>(_initobj);
  for (uint8_t byte : initobj) {
    std::cout << std::hex << (int)byte << " ";
  }
  printf("\n");
  for (const auto &sym : info.symtab) {
    if (ELF64_ST_TYPE(sym.st_info) != STT_FUNC) {
      continue;
    }
    printf("%s\n", info.strtab.data() + sym.st_name);
    auto _obj = translator.get(sym.st_value);
    if (std::holds_alternative<int>(_obj)) {
      puts("error: could not fetch object");
      return std::get<int>(_obj);
    }
    std::vector<uint8_t> obj = std::get<std::vector<uint8_t>>(_obj);
    // dump to file
    FILE *f = fopen(
        (std::string(info.strtab.data() + sym.st_name) + ".o").c_str(), "wb");
    fwrite(obj.data(), 1, obj.size(), f);
    fclose(f);

    if (strcmp(info.strtab.data() + sym.st_name, "fac")) {
      continue;
    }

    llvm::LLVMContext context;
    llvm::MemoryBufferRef mbr({(const char *)obj.data(), obj.size()},
                              info.strtab.data() + sym.st_name);
    llvm::SMDiagnostic err;
    auto mod = llvm::parseIR(mbr, err, context);
    if (!mod) {
      std::cout << "Error parsing module\n";
      err.print("static-client", llvm::errs());
      return 1;
    }
    std::cout << "Trying to print module:\n";
    mod->print(llvm::errs(), nullptr);
    std::cout << "Printed module\n";
  }
}
