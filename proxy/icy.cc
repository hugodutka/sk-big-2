#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include "types.hh"

using namespace std;

class ICYStream {
 private:
  static u32 const bufsz = 32768;
  string host;
  string resource;
  u32 port;
  u32 timeout;
  bool request_meta;

  string request;
  u8 buf[bufsz];

  string build_request() {
    stringstream req;
    req << "GET " << resource << " HTTP/1.0\r\n"
        << "Host: " << host << "\r\n"
        << "User-Agent: radio-proxy\r\n";
    if (request_meta) req << "Icy-MetaData: 1\r\n";
    req << "\r\n";
    return req.str();
  }

 public:
  ICYStream(string const& host, string const& resource, u32 port, u32 timeout, bool request_meta)
      : host(host), resource(resource), port(port), timeout(timeout), request_meta(request_meta) {
    request = build_request();
  }

  void test() { cout << request << endl; }
};
