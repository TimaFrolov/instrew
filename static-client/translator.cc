#include "translator.h"
#include "libc-extra.h"
#include <cstdlib>
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int Translator::hdr_send(uint32_t id, int32_t sz) {
  if (this->last_hdr.id != Msg::UNKNOWN)
    return -EPROTO;
  int ret;
  Msg::Hdr hdr = {id, sz};
  if ((ret = write_full(this->socket, &hdr, sizeof(hdr))) != sizeof(hdr))
    return ret;
  return 0;
}

int32_t Translator::hdr_recv(uint32_t id) {
  if (this->last_hdr.id == Msg::UNKNOWN) {
    int ret = read_full(this->socket, &this->last_hdr, sizeof(this->last_hdr));
    if (ret != sizeof(this->last_hdr))
      return ret;
  }
  if (this->last_hdr.id != id)
    return -EPROTO;
  int32_t sz = this->last_hdr.sz;
  this->last_hdr = {Msg::UNKNOWN, 0};
  return sz;
}

int Translator::init(const char *server_config,
                     const TranslatorServerConfig &tsc) {
  int socket = 0;
  for (size_t i = 0; server_config[i]; i++)
    socket = socket * 10 + server_config[i] - '0';
  this->socket = socket;

  this->written_bytes = 0;
  this->last_hdr = {Msg::UNKNOWN, 0};

  int ret;
  if ((ret = this->hdr_send(Msg::C_INIT, sizeof tsc)))
    return ret;
  if ((ret = write_full(this->socket, &tsc, sizeof tsc)) != sizeof tsc)
    return ret;

  return 0;
}

int Translator::fini() {
  close(this->socket);
  return 0;
}

std::variant<TranslatorConfig, int> Translator::config_fetch() {
  TranslatorConfig cfg;
  int32_t sz = this->hdr_recv(Msg::S_INIT);
  if (sz < 0)
    return sz;
  if (sz != sizeof cfg)
    return -EPROTO;

  ssize_t ret = read_full(this->socket, &cfg, sz);
  if (ret != (ssize_t)sz)
    return static_cast<int>(ret);
  return cfg;
}

std::variant<std::vector<uint8_t>, int> Translator::get_object() {
  int32_t sz = this->hdr_recv(Msg::S_OBJECT);
  if (sz < 0)
    return sz;

  std::vector<uint8_t> buf(sz);

  int ret = read_full(this->socket, buf.data(), sz);
  if (ret != (ssize_t)sz)
    return ret;

  return buf;
}

std::variant<std::vector<uint8_t>, int> Translator::get(uintptr_t addr) {
  int ret;
  if ((ret = this->hdr_send(Msg::C_TRANSLATE, 8)) != 0)
    return ret;
  if ((ret = write_full(this->socket, &addr, sizeof(addr))) != sizeof(addr))
    return ret;

  while (true) {
    int32_t sz = this->hdr_recv(Msg::S_MEMREQ);
    if (sz == -EPROTO) {
      return this->get_object();
    } else if (sz < 0) {
      return sz;
    }

    // handle memory request
    struct {
      uint64_t addr;
      size_t buf_sz;
    } memrq;
    if (sz != sizeof(memrq))
      return -1;
    if ((ret = read_full(this->socket, &memrq, sizeof(memrq))) != sizeof(memrq))
      return ret;
    if (memrq.buf_sz > 0x1000)
      memrq.buf_sz = 0x1000;

    if ((ret = this->hdr_send(Msg::C_MEMBUF, memrq.buf_sz + 1)) < 0)
      return ret;

    uint8_t failed = 0;
    if ((ret = write_full(this->socket, (uint8_t *)elf.data() + memrq.addr, memrq.buf_sz)) !=
        (ssize_t)memrq.buf_sz) {
      // Gracefully handle reads from invalid addresses
      if (ret == -EFAULT) {
        failed = 1;
        // Send zero bytes as padding
        for (size_t i = 0; i < memrq.buf_sz; i++)
          if (write_full(this->socket, "", 1) != 1)
            return ret;
      } else {
        std::cerr << "translator_get: failed writing from address" << memrq.addr
                  << '\n';
        return ret;
      }
    }

    if ((ret = write_full(this->socket, &failed, 1)) != 1)
      return ret;

    this->written_bytes += memrq.buf_sz;
  }
}

int Translator::fork_prepare() {
  int ret;
  if ((ret = this->hdr_send(Msg::C_FORK, 0)))
    return ret;

  int32_t sz = this->hdr_recv(Msg::S_FD);
  if (sz != 4)
    return sz < 0 ? sz : -EPROTO;

  int error;
  struct iovec iov = {&error, sizeof(error)};
  struct fd_cmsg {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
    int fd;
  } cmsg;
  size_t cmsg_len = offsetof(struct fd_cmsg, fd) + sizeof(int);
  struct msghdr msg = {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = &cmsg,
      .msg_controllen = cmsg_len,
  };
  if ((ret = recvmsg(this->socket, &msg, MSG_CMSG_CLOEXEC)) != sizeof(error))
    return ret < 0 ? ret : -EPROTO;
  if (error != 0)
    return error;
  if (cmsg.cmsg_type != SCM_RIGHTS || cmsg.cmsg_len != cmsg_len)
    return -EPROTO;

  return cmsg.fd;
}

int Translator::fork_finalize(int fork_fd) {
  close(this->socket); // Forked process should not use parent translator.
  this->socket = fork_fd;
  return 0;
}
