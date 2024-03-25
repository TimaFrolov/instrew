#pragma once

#include <unistd.h>

#define ALIGN_DOWN(v, a) ((v) & ~((a)-1))
#define ALIGN_UP(v, a) (((v) + (a - 1)) & ~((a)-1))

ssize_t read_full(int fd, void *buf, size_t nbytes);

ssize_t write_full(int fd, const void *buf, size_t nbytes);
