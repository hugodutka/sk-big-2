#ifndef MODEL_HH
#define MODEL_HH

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include "events.hh"
#include "proxy.hh"
#include "telnet.hh"
#include "ui.hh"

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

  int cursor_line;
  vector<ProxyInfo> proxies;

 public:
  Model(u16 telnet_port, const string& proxy_host, u16 proxy_port,
        volatile sig_atomic_t* keep_running)
      : keep_running(keep_running) {
    telnet = make_shared<TelnetServer>(telnet_port);
    cursor_line = 0;
    proxies.push_back(ProxyInfo("Dobreradio", "Sanah - Krolowa dram", 14, true));
    proxies.push_back(ProxyInfo("Zlote przeboje", "Philter - Blue eyes", 15, false));
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
    int num_options = 2 + proxies.size();
    if (in.size() >= 3 && in[0] == 65 && in[1] == 91 && in[2] == 27) {  // up arrow
      cursor_line = cursor_line - 1;
    } else if (in.size() >= 3 && in[0] == 66 && in[1] == 91 && in[2] == 27) {  // down arrow
      cursor_line = cursor_line + 1;
    } else if (in.size() >= 2 && in[0] == 0 && in[1] == 13) {  // enter
      if (cursor_line == 1) {
        cerr << "szukaj pośrednika" << endl;
      } else if (cursor_line == num_options) {
        *keep_running = 0;
      } else {
        int proxy_index = cursor_line - 2;
        auto& proxy_info = proxies[proxy_index];
        for (auto& proxy : proxies) {
          proxy.active = false;
        }
        proxy_info.active = true;
      }
    } else {
      // unrecognized input, do nothing
    }
    cursor_line = max(1, cursor_line);
    cursor_line = min(num_options, cursor_line);
    string ui = generate_ui(proxies);
    telnet->render(ui, cursor_line);
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
