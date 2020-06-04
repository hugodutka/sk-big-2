#ifndef EVENTS_HH
#define EVENTS_HH

#include "types.hh"

enum EventType {
  USER_INPUT,
  PROXY_DISCOVERED,
};

struct Event {
  EventType type;
  virtual void makeMePolymorhic(){};
};

struct EventUserInput : public Event {
  EventType type = USER_INPUT;
  u8 input;
  EventUserInput(u8 input) : input(input) {}
};

struct EventProxyDiscovered : public Event {
  EventType type = PROXY_DISCOVERED;
  u64 id;
  EventProxyDiscovered(u64 id) : id(id) {}
};

#endif
