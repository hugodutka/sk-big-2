#ifndef PROXY_INFO_HH
#define PROXY_INFO_HH

#include <string>
#include "types.hh"

struct ProxyInfo {
  string info;
  string meta;
  u64 id;
  i64 last_contact;
  bool active;

  ProxyInfo(const string& info, const string& meta, u64 id, i64 last_contact, bool active)
      : info(info), meta(meta), id(id), last_contact(last_contact), active(active) {}
};

#endif
