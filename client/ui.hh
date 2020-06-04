#ifndef UI_HH
#define UI_HH

#include <sstream>
#include <string>
#include <vector>
#include "proxy.hh"

using namespace std;

string generate_ui(vector<ProxyInfo>& proxies) {
  stringstream buf;
  buf << "Szukaj pośrednika\r\n";
  for (auto& proxy : proxies) {
    buf << "Pośrednik " << proxy.info << (proxy.active ? " *\r\n" : "\r\n");
  }
  buf << "Koniec\r\n";
  for (auto& proxy : proxies) {
    if (proxy.active) {
      buf << proxy.meta << "\r\n";
    }
  }
  return buf.str();
};

#endif
