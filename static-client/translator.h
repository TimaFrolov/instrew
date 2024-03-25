#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace Msg {

enum Id {
#define INSTREW_MESSAGE_ID(id, name) name = id,
#include "instrew-protocol.inc"
#undef INSTREW_MESSAGE_ID
};

struct Hdr {
  uint32_t id;
  int32_t sz;
} __attribute__((packed));
} // namespace Msg

struct TranslatorServerConfig {
#define INSTREW_SERVER_CONF
#define INSTREW_SERVER_CONF_BOOL(id, name, default) bool tsc_##name = default;
#define INSTREW_SERVER_CONF_INT32(id, name, default)                           \
  int32_t tsc_##name = default;
#include "instrew-protocol.inc"
#undef INSTREW_SERVER_CONF
#undef INSTREW_SERVER_CONF_BOOL
#undef INSTREW_SERVER_CONF_INT32
};

struct TranslatorConfig {
#define INSTREW_CLIENT_CONF
#define INSTREW_CLIENT_CONF_INT32(id, name) int32_t tc_##name;
#include "instrew-protocol.inc"
#undef INSTREW_CLIENT_CONF
#undef INSTREW_CLIENT_CONF_INT32
} __attribute__((packed));

struct Translator {
  int socket;

  size_t written_bytes;
  Msg::Hdr last_hdr;

  std::span<const uint8_t> elf;

  int init(const char *server_config, const TranslatorServerConfig &tsc);
  int fini();
  std::variant<std::vector<uint8_t>, int> get_object();
  std::variant<std::vector<uint8_t>, int> get(uintptr_t addr);
  std::variant<TranslatorConfig, int> config_fetch();
  int fork_prepare();
  int fork_finalize(int fork_fd);

private:
  int hdr_send(uint32_t id, int32_t sz);
  int32_t hdr_recv(uint32_t id);
};
