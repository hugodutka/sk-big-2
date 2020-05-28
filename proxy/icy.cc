#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
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
  conn_t sock;

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

  void setup_connection() {
    struct addrinfo addr_hints, *addr_result;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) throw "socket init failed";

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), to_string(port).c_str(), &addr_hints, &addr_result)) {
      throw "getddrinfo failed";
    }
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen)) {
      freeaddrinfo(addr_result);
      throw "connect failed";
    }
    freeaddrinfo(addr_result);
  }

  void close_connection() noexcept {
    if (sock >= 0) close(sock);
    sock = -1;
  }

 public:
  ICYStream(string const& host, string const& resource, u32 port, u32 timeout, bool request_meta)
      : host(host), resource(resource), port(port), timeout(timeout), request_meta(request_meta) {
    request = build_request();
    sock = -1;
  }

  ~ICYStream() { close_connection(); }

  void test() { cout << request << endl; }
};
