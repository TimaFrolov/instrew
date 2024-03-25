#include "libc-extra.h"
#include <cstdint>
#include <errno.h>

ssize_t read_full(int fd, void *buf, size_t nbytes) {
  size_t total_read = 0;
  uint8_t *buf_cp = static_cast<uint8_t *>(buf);
  while (total_read < nbytes) {
    ssize_t bytes_read = read(fd, buf_cp + total_read, nbytes - total_read);
    if (bytes_read < 0)
      return bytes_read;
    if (bytes_read == 0)
      return -EIO;
    total_read += bytes_read;
  }
  return total_read;
}

ssize_t write_full(int fd, const void *buf, size_t nbytes) {
  size_t total_written = 0;
  const uint8_t *buf_cp = static_cast<const uint8_t *>(buf);
  while (total_written < nbytes) {
    ssize_t bytes_written =
        write(fd, buf_cp + total_written, nbytes - total_written);
    if (bytes_written < 0)
      return bytes_written;
    if (bytes_written == 0)
      return -EIO;
    total_written += bytes_written;
  }
  return total_written;
}
