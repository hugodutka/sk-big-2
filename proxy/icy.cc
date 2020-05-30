#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include "types.hh"

using namespace std;

struct ICYPart {
  size_t size;
  bool meta_present;
  string meta;

  ICYPart(size_t size, bool meta_present = false, string meta = "")
      : size(size), meta_present(meta_present), meta(meta) {}
};

class ICYStream {
 private:
  string host;
  string resource;
  u32 port;
  u32 timeout;
  bool request_meta;

  static u32 const bufsz = 32768;
  conn_t sock;
  string request;
  size_t meta_offset;
  size_t remaining_chunk_size;

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
    if (sock < 0) throw runtime_error("socket init failed");

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), to_string(port).c_str(), &addr_hints, &addr_result)) {
      throw runtime_error("getaddrinfo failed");
    }
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen)) {
      freeaddrinfo(addr_result);
      throw runtime_error("connect failed");
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

    if (status != 0) throw runtime_error("setsockopt failed");
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
      if (bytes_sent < 0) throw runtime_error("write failed");
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
        throw runtime_error("failed to read a character");
      }
      if (c == EOF || c == '\0') {
        throw runtime_error("read invalid character");
      }
      line << c;
      stop = prev_c == '\r' && c == '\n';
      prev_c = c;
    }

    return line.str();
  }

  void parse_headers() {
    static std::regex rg_status("^(ICY 200 OK|HTTP\\/1.0 200 OK|HTTP\\/1.1 200 OK)\r\n$",
                                regex_constants::ECMAScript | regex_constants::icase);
    static std::regex rg_meta("^icy-metaint:\\s*([0-9]+)\r\n$",
                              regex_constants::ECMAScript | regex_constants::icase);

    string header = read_header();
    if (!regex_match(header, rg_status)) throw runtime_error("invalid status line");

    smatch match_groups;
    bool meta_found = false;
    while (header != "\r\n") {
      header = read_header();
      if (request_meta && regex_match(header, match_groups, rg_meta)) {
        meta_offset = (size_t)stoul(match_groups[1]);
        meta_found = true;
      }
    }

    if (request_meta && !meta_found) request_meta = false;
    if (!request_meta && meta_found) throw runtime_error("server sent an unsupported meta header");
  }

  string read_meta() {}

 public:
  ICYStream(string const& host, string const& resource, u32 port, u32 timeout, bool request_meta)
      : host(host), resource(resource), port(port), timeout(timeout), request_meta(request_meta) {
    request = build_request();
    sock = -1;
    remaining_chunk_size = 0;
    meta_offset = 16384;  // default
  }

  ~ICYStream() { close_connection(); }

  void open() {
    if (sock < 0) setup_connection();
  }

  size_t get_chunk_size() { return meta_offset; }

  ICYPart read_chunk(u8* buf) {
    size_t chunk_size = remaining_chunk_size > 0 ? remaining_chunk_size : meta_offset;
    ssize_t num_read = read(sock, buf, chunk_size);
    if (num_read < 0) throw runtime_error("read failed");
    if (num_read == 0) throw runtime_error("connection closed");

    remaining_chunk_size = chunk_size - num_read;
    return ICYPart(chunk_size - remaining_chunk_size, request_meta && remaining_chunk_size == 0,
                   "");
  }

  void test() {
    setup_connection();
    string request = build_request();
    send(request);
    parse_headers();

    size_t chunk_size = get_chunk_size();
    shared_ptr<u8[]> chunk(new u8[chunk_size]);

    for (u64 i = 0; i < 10; i++) {
      ICYPart part = read_chunk(chunk.get());
      cout << "read " << part.size << " bytes. has metadata: " << part.meta_present << endl;
    }
  }
};
