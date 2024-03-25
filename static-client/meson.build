sources = [
  'main.cc',
  'elf-loader.cc',
  'libc-extra.cc',
  'translator.cc',
]

instrew = executable('instrew', sources, 
                     include_directories: include_directories('.', '../shared'),
                     cpp_args: ['-std=c++20'],
                     dependencies: [libllvm],
                     link_args: ['-ldl'],
                     install: true)

mytarget = custom_target('factorial RISCV',
  output : 'factorial',
  input : 'factorial.S' ,
  build_by_default: true,
  command : ['riscv64-unknown-linux-gnu-gcc', '-march=rv64imd', '-nostdlib', '-static', '@INPUT@', '-o', '@OUTPUT@']
)
