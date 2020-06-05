#ifndef EVENTS_HH
#define EVENTS_HH

#include <memory>
#include <stdexcept>
#include <vector>
#include "proxyinfo.hh"
#include "types.hh"

struct Event {
  virtual void makeMePolymorphic(){};  // a virtual method needs to be here for dynamic_cast to work
};

struct EventUserInput : public Event {
  u8 input;
  EventUserInput(u8 input) : input(input) {}
};

struct EventProxiesChanged : public Event {
  shared_ptr<vector<ProxyInfo>> proxies;
  EventProxiesChanged(shared_ptr<vector<ProxyInfo>> proxies) : proxies(proxies) {}
};

struct EventProxyManagerCrashed : public Event {
  exception_ptr exc;
  EventProxyManagerCrashed(exception_ptr exc) : exc(exc) {}
};

#endif
