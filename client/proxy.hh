#ifndef PROXY_HH
#define PROXY_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include "events.hh"
#include "types.hh"
#include "utils.hh"

using namespace std;

// UDP message types
constexpr u16 DISCOVER = 1;
constexpr u16 IAM = 2;
constexpr u16 KEEPALIVE = 3;
constexpr u16 AUDIO = 4;
constexpr u16 METADATA = 6;

constexpr size_t HEADER_SIZE = 4;

class ProxyManager {
 private:
  string host;
  u16 port;

  conn_t sock;
  sockaddr_in my_address;
  sockaddr_in proxy_address;
  sockaddr_in msg_sender;

  static const size_t msg_buf_size = 65568;
  u8 msg_buf[msg_buf_size];
  ssize_t msg_len;

  function<void(shared_ptr<Event>)> notify;

  // Tries to read a message from sock into msg_buf. Saves the address of the sender in msg_sender.
  // Returns true if a message was read, false otherwise.
  bool receive_msg() {
    static socklen_t addrlen = sizeof(struct sockaddr);

    errno = 0;
    ssize_t read_len = recvfrom(sock, &msg_buf, msg_buf_size, 0, (sockaddr*)&msg_sender, &addrlen);

    if (read_len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
      throw runtime_error("recvfrom failed");
    }
    msg_len = read_len;
    return true;
  }

  void send_msg(const sockaddr* addr, u16 msg_type, const u8* msg, size_t len) {
    static u8 buf[msg_buf_size];
    static socklen_t socklen = sizeof(struct sockaddr);
    size_t sent = 0;

    ((u16*)(buf))[0] = htons(msg_type);
    ((u16*)(buf))[1] = htons(static_cast<u16>(len));
    memcpy(buf + HEADER_SIZE, msg, len);
    len += HEADER_SIZE;

    while (sent < len) {
      ssize_t sent_partial = sendto(sock, buf + sent, len, 0, addr, socklen);
      if (sent_partial <= 0) throw runtime_error("sendto failed");
      sent += static_cast<size_t>(sent_partial);
    }
  }

  // Processes a message that is in msg_buf.
  void process_msg() {
    if (msg_len < HEADER_SIZE) throw runtime_error("invalid message");
    u16 msg_type = ntohs(((u16*)(&msg_buf))[0]);
    u16 msg_content_len = ntohs(((u16*)(&msg_buf))[1]);
    u8* msg_content = msg_buf + HEADER_SIZE;

    i64 current_time = now();
    u64 sender_id = hash_sockaddr_in(msg_sender);

    if (msg_type == IAM) {
      notify(make_shared<EventIamSent>(sender_id, current_time, string((char*)(msg_content))));
    } else if (msg_type == AUDIO) {
      auto audio = shared_ptr<u8[]>(new u8[msg_content_len]);
      memcpy(audio.get(), msg_content, msg_content_len);
      notify(make_shared<EventAudioSent>(sender_id, current_time, audio, msg_content_len));
    } else if (msg_type == METADATA) {
      notify(make_shared<EventMetaSent>(sender_id, current_time, string((char*)(msg_content))));
    } else {
      throw runtime_error("unexpected message type: " + to_string(msg_type));
    }
  }

  void init_my_address() {
    timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = (suseconds_t)100000;  // 100 ms

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) throw runtime_error("socket failed");

    my_address.sin_family = AF_INET;
    my_address.sin_addr.s_addr = htonl(INADDR_ANY);
    my_address.sin_port = htons(port);

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&read_timeout, sizeof(read_timeout)) < 0)
      throw runtime_error("setsockopt set timeout failed");
  }

  void init_proxy_address() {
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
      throw runtime_error("setsockopt broadcast failed");

    optval = 69;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
      throw runtime_error("setsockopt multicast ttl failed");

    struct addrinfo addr_hints;
    struct addrinfo* addr_result;

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;

    if (getaddrinfo(host.c_str(), NULL, &addr_hints, &addr_result) != 0)
      throw runtime_error("getaddrinfo failed");

    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = ((sockaddr_in*)(addr_result->ai_addr))->sin_addr.s_addr;
    proxy_address.sin_port = htons(port);

    freeaddrinfo(addr_result);
  }

 public:
  ProxyManager(const string& host, u16 port, function<void(shared_ptr<Event>)> notify)
      : host(host), port(port), notify(notify) {
    memset(&msg_buf, 0, msg_buf_size);
  }

  void init() {
    init_my_address();
    init_proxy_address();
  }

  void clean_up() {
    if (sock >= 0) close(sock);
    sock = -1;
  }

  void discover_proxies() { send_msg((sockaddr*)(&proxy_address), DISCOVER, nullptr, 0); }

  void start(volatile sig_atomic_t* keep_running) {
    try {
      while (*keep_running) {
        if (receive_msg()) {
          cerr << "got a message!" << endl;
          try {
            process_msg();
          } catch (exception& e) {
            cerr << "process_msg failed - skipping the message. Failure reason:" << endl;
            cerr << e.what() << endl;
          }
        }
      }
    } catch (...) {
      auto event = make_shared<EventProxyManagerCrashed>(current_exception());
      notify(event);
      throw;
    }
  }
};

#endif
