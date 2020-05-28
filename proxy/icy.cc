#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
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
    struct timeval connection_timeout;

    connection_timeout.tv_sec = (time_t)timeout;
    connection_timeout.tv_usec = 0;
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) throw "socket init failed";

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), to_string(port).c_str(), &addr_hints, &addr_result)) {
      throw "getaddrinfo failed";
    }
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen)) {
      freeaddrinfo(addr_result);
      throw "connect failed";
    }

    int status = 0;
    try {
      status += setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void*)&connection_timeout,
                           sizeof(connection_timeout));
      status += setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&connection_timeout,
                           sizeof(connection_timeout));
    } catch (...) {
    }
    freeaddrinfo(addr_result);

    if (status != 0) throw "setsockopt failed";
  }

  void close_connection() noexcept {
    if (sock >= 0) close(sock);
    sock = -1;
  }

  void send(const string& msg) {
    ssize_t sent_total = 0, bytes_sent = 0;
    const char* msg_ptr = msg.c_str();
    while ((size_t)sent_total < msg.length()) {
      bytes_sent = write(sock, msg_ptr + bytes_sent, msg.length() - bytes_sent);
      if (bytes_sent < 0) throw "write failed";
      sent_total += bytes_sent;
    };
  }

  string read_header() {
    stringstream line;

    errno = 0;
    ssize_t num_read = 0;
    bool stop = false;
    char c, prev_c = '\0';  // arbitrary default other than '\r'
    while (!stop) {
      num_read = read(sock, &c, 1);
      if (num_read != 1) {
        throw "failed to read a character";
      }
      if (c == EOF || c == '\0') {
        throw "read invalid character";
      }
      line << c;
      stop = prev_c == '\r' && c == '\n';
      prev_c = c;
    }

    return line.str();
  }

  void parse_headers() {
    string header = "";
    while (header != "\r\n") {
      header = read_header();
      cout << header;
    }
  }

 public:
  ICYStream(string const& host, string const& resource, u32 port, u32 timeout, bool request_meta)
      : host(host), resource(resource), port(port), timeout(timeout), request_meta(request_meta) {
    request = build_request();
    sock = -1;
  }

  ~ICYStream() { close_connection(); }

  void test() {
    setup_connection();
    string request = build_request();
    send(request);
    parse_headers();
  }
};
