#ifndef PROXY_INFO_HH
#define PROXY_INFO_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include "types.hh"

struct ProxyInfo {
  string info;
  string meta;
  u64 id;
  i64 last_contact;
  bool active;
  sockaddr_in addr;

  ProxyInfo(const string& info, const string& meta, u64 id, i64 last_contact, bool active,
            const sockaddr_in& addr)
      : info(info), meta(meta), id(id), last_contact(last_contact), active(active), addr(addr) {}
};

#endif
