#ifndef MODEL_HH
#define MODEL_HH

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include "events.hh"
#include "telnet.hh"

using namespace std;
using namespace chrono_literals;

class Model {
 private:
  shared_ptr<TelnetServer> telnet;
  deque<u8> input_buf;
  queue<shared_ptr<Event>> event_queue;
  mutex lock_mutex;
  condition_variable cv;
  volatile sig_atomic_t* keep_running;
  future<void> telnet_ft;

 public:
  Model(u16 telnet_port, const string& proxy_host, u16 proxy_port,
        volatile sig_atomic_t* keep_running)
      : keep_running(keep_running) {
    telnet = make_shared<TelnetServer>(telnet_port);
  }

  ~Model() {
    try {
      clean_up();
    } catch (...) {
    }
  }

  void init() {
    telnet->init();
    auto telnet_loop = [this]() {
      telnet->start(keep_running, [this](u8 in) { notify(make_shared<EventUserInput>(in)); });
    };
    telnet_ft = async(launch::async, telnet_loop);
  }

  void clean_up() {
    try {
      telnet->clean_up();
    } catch (...) {
    }
    telnet_ft.get();
  }

  void notify(shared_ptr<Event> event) {
    lock_guard<mutex> lock(lock_mutex);
    event_queue.push(event);
    cv.notify_one();
  }

  void react(EventUserInput* event) {
    input_buf.push_front(event->input);
    while (input_buf.size() > 3) {
      input_buf.pop_back();
    }
    auto& in = input_buf;
    if (in.size() >= 3 && in[0] == 65 && in[1] == 91 && in[2] == 27) {
      // up arrow
      cerr << "up arrow" << endl;
    } else if (in.size() >= 3 && in[0] == 66 && in[1] == 91 && in[2] == 27) {
      // down arrow
      cerr << "down arrow" << endl;
    } else if (in.size() >= 2 && in[0] == 0 && in[1] == 13) {
      // enter
      cerr << "enter" << endl;
    } else {
      // unrecognized input, do nothing
    }
  }

  void react(EventProxyDiscovered* event) {
    // hurr durr
  }

  void process_event_from_queue() {
    if (event_queue.empty()) return;
    auto event = event_queue.front();
    event_queue.pop();
    if (auto ev = dynamic_cast<EventUserInput*>(event.get())) {
      react(ev);
    } else if (auto ev = dynamic_cast<EventProxyDiscovered*>(event.get())) {
      react(ev);
    };
  }

  void start() {
    while (*keep_running) {
      unique_lock<mutex> lock(lock_mutex);
      cv.wait_for(lock, 100ms, []() {
        return true;
      });  // set timeout to avoid deadlocks that were not foreseen by the author :)
      while (!event_queue.empty()) {
        process_event_from_queue();
      }
    }
  }
};

#endif
