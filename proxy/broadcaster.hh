#ifndef BROADCASTER_HH
#define BROADCASTER_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
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
  bool multicast_initialized;
  conn_t sock;
  sockaddr_in address;
  struct ip_mreq ip_mreq;

  static const size_t msg_buf_size = 65568;
  static const size_t chunk_buf_size = 1024 + HEADER_SIZE;
  u8 msg_buf[msg_buf_size];
  u8 chunk_buf[chunk_buf_size];
  ssize_t msg_len;
  sockaddr_in msg_sender;

  unordered_map<u64, shared_ptr<ClientInfo>> clients;

  // Tries to read a message from sock into msg_buf. Saves the address of the sender in msg_sender.
  // Returns true if a message was read, false otherwise.
  bool receive_msg() {
    static socklen_t addrlen = sizeof(struct sockaddr);

    errno = 0;
    ssize_t read_len =
        recvfrom(sock, &msg_buf, msg_buf_size, MSG_DONTWAIT, (sockaddr*)&msg_sender, &addrlen);

    if (read_len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
      throw runtime_error("recvfrom failed");
    }
    msg_len = read_len;
    return true;
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
      size_t response_len = HEADER_SIZE + radio_info.length();
      shared_ptr<u8[]> response(new u8[response_len]);
      ((u16*)response.get())[0] = IAM;
      ((u16*)response.get())[1] = static_cast<u16>(radio_info.length());
      memcpy(response.get() + HEADER_SIZE, radio_info.c_str(), radio_info.length());
      send_msg((sockaddr*)&msg_sender, response.get(), response_len);
    } else if (msg_type == KEEPALIVE) {
      // do nothing
    } else {
      throw runtime_error("unexpected message type: " + to_string(msg_type));
    }

    clients[hash_sockaddr_in(msg_sender)] = make_shared<ClientInfo>(now(), msg_sender);
  }

  void handle_incoming_msgs() {
    while (receive_msg()) {
      try {
        process_msg();
      } catch (exception& e) {
        cerr << "Could not process an incoming message. Skipping it. Reason:" << endl;
        cerr << e.what() << endl;
      }
    }
  }

  void remove_inactive_clients() {
    i64 disconnect_after_ms = 5000;
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

  void send_to_clients(u16 msg_type, const u8* data, size_t size) {
    size_t chunk_size = chunk_buf_size - HEADER_SIZE;
    size_t remaining_size = size;
    size_t current_chunk_size;
    size_t offset;

    // perform at least one iteration to send empty messages such as metadata
    do {
      offset = size - remaining_size;
      current_chunk_size = min(chunk_size, remaining_size);
      remaining_size -= current_chunk_size;

      ((u16*)(chunk_buf))[0] = msg_type;
      ((u16*)(chunk_buf))[1] = static_cast<u16>(current_chunk_size);
      memcpy(chunk_buf + HEADER_SIZE, data, current_chunk_size);

      for (auto it : clients) {
        send_msg((sockaddr*)(&it.second->addr), chunk_buf, current_chunk_size + HEADER_SIZE);
      }
    } while (remaining_size > 0);
  }

 public:
  // setting `multiaddr` to an empty string disables multicasting
  UDPBroadcaster(u16 port, const string& multiaddr, const string& radio_info)
      : port(port), multiaddr(multiaddr), radio_info(radio_info) {
    sock = -1;
    multicast_initialized = false;

    // 64000 is an arbitrary number that fits into a UDP datagram
    if (radio_info.length() > 64000) throw runtime_error("radio_info is too long");
  }

  void init() override {
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

    if (bind(sock, (struct sockaddr*)&address, sizeof address) < 0)
      throw runtime_error("bind failed");
  }

  void clean_up() override {
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
    static i64 last_check = now();
    i64 current_time = now();
    handle_incoming_msgs();
    remove_inactive_clients();
    send_to_clients(AUDIO, data, part.size);
    if (part.meta_present) {
      send_to_clients(METADATA, (u8*)(part.meta.c_str()), part.meta.length());
    }
  }
};

#endif
