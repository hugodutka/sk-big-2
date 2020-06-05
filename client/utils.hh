#ifndef UTILS_HH
#define UTILS_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <chrono>
#include "types.hh"

using namespace std;

// Returns the current time in milliseconds.
i64 now() {
  return chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch())
      .count();
}

u64 hash_sockaddr_in(const sockaddr_in& addr) {
  u64 port = static_cast<u64>(addr.sin_port);
  u64 ip = static_cast<u64>(addr.sin_addr.s_addr);
  return (ip << 32) + port;
}

#endif
