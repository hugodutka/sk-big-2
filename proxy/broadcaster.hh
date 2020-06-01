#ifndef BROADCASTER_HH
#define BROADCASTER_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include "icy.hh"
#include "types.hh"

using namespace std;

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
  int64_t last_contact;  // time in milliseconds
  sockaddr_in addr;
  ClientInfo(int64_t last_contact, const sockaddr_in& addr)
      : last_contact(last_contact), addr(addr){};
};

class UDPBroadcaster : public Broadcaster {
  u16 port;
  string multiaddr;
  bool multicast_initialized;
  conn_t sock;
  sockaddr_in address;
  struct ip_mreq ip_mreq;

  static const size_t msg_buf_size = 65568;
  u8 msg_buf[msg_buf_size];
  sockaddr_in msg_sender;

  unordered_map<u64, ClientInfo> clients;

  // Reads a message from sock into msg_buf. Saves the address of the sender in msg_sender.
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
    return true;
  }

  u64 hash_sockaddr_in(const sockaddr_in& addr) {
    u64 port = static_cast<u64>(addr.sin_port);
    u64 ip = static_cast<u64>(addr.sin_addr.s_addr);
    return (ip << 32) + port;
  }

  // Returns the current time in milliseconds.
  int64_t now() {
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
  }

 public:
  // setting `multiaddr` to an empty string disables multicasting
  UDPBroadcaster(u16 port, const string& multiaddr) : port(port), multiaddr(multiaddr) {
    sock = -1;
    multicast_initialized = false;
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
    static u64 counter = 0;
    bool msg_received = receive_msg();
    if (msg_received) {
      cout << "I received a UDP message!" << endl;
      cout << "The message is:" << endl;
      cout << string((char*)(&msg_buf)) << endl;
      cout << "Got it from:" << endl;
      auto hash = hash_sockaddr_in(msg_sender);
      cout << hash_sockaddr_in(msg_sender) << endl;
      clients.insert({hash, ClientInfo(now(), msg_sender)});
      cout << "got " << clients.size() << " clients" << endl;
      for (auto it = clients.begin(); it != clients.end(); it++) {
        cout << it->first << ": " << it->second.addr.sin_port << endl;
      }
    }
    // cout << ms << endl;
    // cout << "broadcasting udp " << ++counter << endl;
  }
};

#endif
