#ifndef BROADCASTER_HH
#define BROADCASTER_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
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

class UDPBroadcaster : public Broadcaster {
  u16 port;
  string multiaddr;
  bool multicast_initialized;
  conn_t sock;
  sockaddr_in address;
  struct ip_mreq ip_mreq;

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
    cout << "broadcasting udp " << ++counter << endl;
  }
};

#endif
