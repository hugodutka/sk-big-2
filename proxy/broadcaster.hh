#ifndef BROADCASTER_HH
#define BROADCASTER_HH

#include <iostream>
#include "icy.hh"
#include "types.hh"

using namespace std;

class Broadcaster {
 public:
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

#endif
