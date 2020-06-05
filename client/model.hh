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
#include <utility>
#include "events.hh"
#include "proxy.hh"
#include "telnet.hh"
#include "ui.hh"

using namespace std;
using namespace chrono_literals;

class Model {
 private:
  shared_ptr<TelnetServer> telnet;
  shared_ptr<ProxyManager> proxy_manager;
  deque<u8> input_buf;
  queue<shared_ptr<Event>> event_queue;
  mutex lock_mutex;
  condition_variable cv;
  volatile sig_atomic_t* keep_running;
  future<void> telnet_ft;
  future<void> proxy_manager_ft;

  int cursor_line;
  vector<ProxyInfo> proxies;

  int get_num_menu_options() { return 2 + proxies.size(); }

 public:
  Model(u16 telnet_port, const string& proxy_host, u16 proxy_port, u32 proxy_timeout,
        volatile sig_atomic_t* keep_running)
      : keep_running(keep_running) {
    auto f_notify = [this](auto e) { notify(e); };

    telnet = make_shared<TelnetServer>(telnet_port);
    proxy_manager = make_shared<ProxyManager>(proxy_host, proxy_port, proxy_timeout, f_notify);

    cursor_line = 1;
    proxies.push_back(ProxyInfo("Dobreradio", "Sanah - Krolowa dram", 14, 15, true));
    proxies.push_back(ProxyInfo("Zlote przeboje", "Philter - Blue eyes", 15, 15, false));
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

    proxy_manager->init();
    auto proxy_manager_loop = [this]() { proxy_manager->start(keep_running); };
    proxy_manager_ft = async(launch::async, proxy_manager_loop);
  }

  void clean_up() {
    try {
      telnet->clean_up();
      proxy_manager->clean_up();
    } catch (...) {
    }
    telnet_ft.get();
    proxy_manager_ft.get();
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
    int num_options = get_num_menu_options();
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
  }

  void react(EventProxiesChanged* event) { proxies = *(event->proxies); }

  void react(EventProxyManagerCrashed* event) {
    *keep_running = 0;
    rethrow_exception(event->exc);
  }

  void process_event_from_queue() {
    if (event_queue.empty()) return;
    auto event = event_queue.front();
    event_queue.pop();
    if (auto ev = dynamic_cast<EventUserInput*>(event.get())) {
      react(ev);
    } else if (auto ev = dynamic_cast<EventProxiesChanged*>(event.get())) {
      react(ev);
    } else if (auto ev = dynamic_cast<EventProxyManagerCrashed*>(event.get())) {
      react(ev);
    };
    cursor_line = max(1, cursor_line);
    cursor_line = min(get_num_menu_options(), cursor_line);
    string ui = generate_ui(proxies);
    telnet->render(ui, cursor_line);
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
