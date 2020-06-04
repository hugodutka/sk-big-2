#ifndef PROXY_HH
#define PROXY_HH

#include <string>
#include "types.hh"

using namespace std;

struct ProxyInfo {
  string info;
  string meta;
  u64 id;
  bool active;

  ProxyInfo(const string& info, const string& meta, u64 id, bool active)
      : info(info), meta(meta), id(id), active(active) {}
};

#endif
