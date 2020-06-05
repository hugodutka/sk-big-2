#ifndef EVENTS_HH
#define EVENTS_HH

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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

struct EventIamSent : public Event {
  u64 sender_id;
  i64 timestamp;
  shared_ptr<sockaddr_in> sender;
  shared_ptr<string> iam;
  EventIamSent(u64 sender_id, i64 timestamp, const sockaddr_in& sender_ref, const string& iam_str)
      : sender_id(sender_id), timestamp(timestamp) {
    iam = make_shared<string>(iam_str);
    sender = make_shared<sockaddr_in>(sender_ref);
  }
};

struct EventAudioSent : public Event {
  u64 sender_id;
  i64 timestamp;
  shared_ptr<u8[]> audio;
  size_t length;
  EventAudioSent(u64 sender_id, i64 timestamp, shared_ptr<u8[]> audio, size_t length)
      : sender_id(sender_id), timestamp(timestamp), audio(audio), length(length) {}
};

struct EventMetaSent : public Event {
  u64 sender_id;
  i64 timestamp;
  shared_ptr<string> meta;
  EventMetaSent(u64 sender_id, i64 timestamp, const string& meta_str)
      : sender_id(sender_id), timestamp(timestamp) {
    meta = make_shared<string>(meta_str);
  }
};

struct EventProxyManagerCrashed : public Event {
  exception_ptr exc;
  EventProxyManagerCrashed(exception_ptr exc) : exc(exc) {}
};

#endif
