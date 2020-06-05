#ifndef BROADCASTER_HH
#define BROADCASTER_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include "icy.hh"
#include "types.hh"

using namespace std;

// UDP message types
constexpr u16 DISCOVER = 1;
constexpr u16 IAM = 2;
constexpr u16 KEEPALIVE = 3;
constexpr u16 AUDIO = 4;
constexpr u16 METADATA = 6;
constexpr size_t HEADER_SIZE = 4;

class Broadcaster {
 public:
  virtual void init(){};
  virtual void clean_up(){};
  virtual void broadcast(const ICYPart& part, const u8* data) = 0;
};

class StdoutBroadcaster : public Broadcaster {
 public:
  virtual void broadcast(const ICYPart& part, const u8* data) override {
    for (size_t i = 0; i < part.size; i++) {
      cout << data[i];
    }
    if (part.meta_present) {
      cerr << part.meta;
    }
  }
};

struct ClientInfo {
  i64 last_contact;  // time in milliseconds
  sockaddr_in addr;
  ClientInfo(i64 last_contact, const sockaddr_in& addr) : last_contact(last_contact), addr(addr){};
};

class UDPBroadcaster : public Broadcaster {
  u16 port;
  string multiaddr;
  string radio_info;
  u32 timeout;
  bool multicast_initialized;
  conn_t sock;
  sockaddr_in address;
  struct ip_mreq ip_mreq;

  static const size_t msg_buf_size = 65568;
  u8 msg_buf[msg_buf_size];
  ssize_t msg_len;
  sockaddr_in msg_sender;

  unordered_map<u64, shared_ptr<ClientInfo>> clients;

  mutex lock;
  thread udp_server;
  atomic<bool> udp_server_enabled;
  atomic<bool> udp_server_crashed;
  exception_ptr udp_server_exception;

  string last_meta;

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

  pair<shared_ptr<u8[]>, size_t> prepare_msg(u16 msg_type, const u8* data, size_t len) {
    size_t msg_len = HEADER_SIZE + len;
    shared_ptr<u8[]> msg(new u8[msg_len]);
    ((u16*)msg.get())[0] = htons(msg_type);
    ((u16*)msg.get())[1] = htons(static_cast<u16>(len));
    memcpy(msg.get() + HEADER_SIZE, data, len);
    return {msg, msg_len};
  }

  void send_msg(const sockaddr* addr, const u8* msg, size_t len) {
    static socklen_t socklen = sizeof(struct sockaddr);
    size_t sent = 0;

    while (sent < len) {
      ssize_t sent_partial = sendto(sock, msg + sent, len, 0, addr, socklen);
      if (sent_partial <= 0) throw runtime_error("sendto failed");
      sent += static_cast<size_t>(sent_partial);
    }
  }

  u64 hash_sockaddr_in(const sockaddr_in& addr) {
    u64 port = static_cast<u64>(addr.sin_port);
    u64 ip = static_cast<u64>(addr.sin_addr.s_addr);
    return (ip << 32) + port;
  }

  // Returns the current time in milliseconds.
  i64 now() {
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
  }

  // Processes a message that is in msg_buf.
  void process_msg() {
    if (msg_len != HEADER_SIZE) throw runtime_error("invalid message");
    u16 msg_type = ntohs(((u16*)(&msg_buf))[0]);
    u16 msg_content_len = ntohs(((u16*)(&msg_buf))[1]);
    if (msg_content_len != 0) throw runtime_error("invalid message length");

    if (msg_type == DISCOVER) {
      // Send back an IAM message
      auto [response, response_len] =
          prepare_msg(IAM, (u8*)radio_info.c_str(), radio_info.length());
      send_msg((sockaddr*)&msg_sender, response.get(), response_len);
      // Send a METADATA message
      auto [meta_msg, meta_msg_len] =
          prepare_msg(METADATA, (u8*)last_meta.c_str(), last_meta.length());
      send_msg((sockaddr*)&msg_sender, meta_msg.get(), meta_msg_len);
    } else if (msg_type == KEEPALIVE) {
      // do nothing
    } else {
      throw runtime_error("unexpected message type: " + to_string(msg_type));
    }

    clients[hash_sockaddr_in(msg_sender)] = make_shared<ClientInfo>(now(), msg_sender);
  }

  void remove_inactive_clients() {
    i64 disconnect_after_ms = timeout * 1000;
    i64 current_time = now();
    auto it = clients.begin();
    while (it != clients.end()) {
      if (current_time - it->second->last_contact > disconnect_after_ms) {
        it = clients.erase(it);
      } else {
        it++;
      }
    }
  }

  void start_udp_server() {
    try {
      while (udp_server_enabled) {
        try {
          bool msg_received = receive_msg();
          lock_guard<mutex> lock_g(lock);
          if (msg_received) process_msg();
          remove_inactive_clients();
        } catch (exception& e) {
          cerr << "Could not process an incoming message. Skipping it. Reason:" << endl;
          cerr << e.what() << endl;
        }
      }
    } catch (...) {
      lock_guard<mutex> lock_g(lock);
      udp_server_crashed = true;
      udp_server_enabled = false;
      udp_server_exception = current_exception();
    }
  }

  void send_to_clients(u16 msg_type, const u8* data, size_t size) {
    size_t chunk_size = 1024;
    size_t remaining_size = size;
    size_t current_chunk_size;
    size_t offset;

    // perform at least one iteration to send empty messages
    do {
      offset = size - remaining_size;
      current_chunk_size = min(chunk_size, remaining_size);
      remaining_size -= current_chunk_size;

      auto [msg, msg_len] = prepare_msg(msg_type, data + offset, current_chunk_size);

      for (auto it : clients) {
        send_msg((sockaddr*)(&it.second->addr), msg.get(), msg_len);
      }
    } while (remaining_size > 0);
  }

 public:
  // setting `multiaddr` to an empty string disables multicasting
  UDPBroadcaster(u16 port, const string& multiaddr, const string& radio_info, u32 timeout)
      : port(port), multiaddr(multiaddr), radio_info(radio_info), timeout(timeout) {
    sock = -1;
    multicast_initialized = false;
    udp_server_enabled = false;
    udp_server_crashed = false;
    last_meta = "";

    // 64000 is an arbitrary number that fits into a UDP datagram
    if (radio_info.length() > 64000) throw runtime_error("radio_info is too long");
  }

  void init() override {
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = (suseconds_t)100000;  // 100 ms

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) throw runtime_error("socket failed");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (multiaddr != "") {
      ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      if (inet_aton(multiaddr.c_str(), &ip_mreq.imr_multiaddr) == 0)
        throw runtime_error("inet_aton failed");
      if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
        throw runtime_error("setsockopt add membership failed");
      multicast_initialized = true;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&read_timeout, sizeof(read_timeout)) < 0)
      throw runtime_error("setsockopt set timeout failed");

    if (bind(sock, (struct sockaddr*)&address, sizeof address) < 0)
      throw runtime_error("bind failed");

    udp_server_enabled = true;
    udp_server = thread([this] { start_udp_server(); });
  }

  void clean_up() override {
    udp_server_enabled = false;
    udp_server.join();

    if (multicast_initialized &&
        setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0) {
      throw runtime_error("setsockopt ip drop membership failed");
    }
    multicast_initialized = false;

    if (sock >= 0) close(sock);
    sock = -1;
  }

  ~UDPBroadcaster() {
    try {
      clean_up();
    } catch (...) {
      // ignore errors since this is a destructor
    }
  }

  virtual void broadcast(const ICYPart& part, const u8* data) override {
    lock_guard<mutex> lock_g(lock);
    send_to_clients(AUDIO, data, part.size);
    if (part.meta_present && part.meta.size() > 0) {
      send_to_clients(METADATA, (u8*)(part.meta.c_str()), part.meta.length());
      last_meta = part.meta;
    }
    if (udp_server_crashed) rethrow_exception(udp_server_exception);
  }
};

#endif
