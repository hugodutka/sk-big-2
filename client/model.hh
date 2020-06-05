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
#include <vector>
#include "events.hh"
#include "proxy.hh"
#include "proxyinfo.hh"
#include "telnet.hh"
#include "ui.hh"

using namespace std;
using namespace chrono_literals;

class Model {
 private:
  u32 proxy_timeout;

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
  unordered_map<u64, shared_ptr<ProxyInfo>> proxies;

  int get_num_menu_options() { return 2 + proxies.size(); }

  vector<u64> get_ordered_proxy_ids() {
    vector<u64> ids;
    for (auto& proxy : proxies) {
      ids.push_back(proxy.first);
    }
    sort(ids.begin(), ids.end());
    return ids;
  }

 public:
  Model(u16 telnet_port, const string& proxy_host, u16 proxy_port, u32 proxy_timeout,
        volatile sig_atomic_t* keep_running)
      : proxy_timeout(proxy_timeout), keep_running(keep_running) {
    auto f_notify = [this](auto e) { notify(e); };

    telnet = make_shared<TelnetServer>(telnet_port, f_notify);
    proxy_manager = make_shared<ProxyManager>(proxy_host, proxy_port, f_notify);

    cursor_line = 1;
  }

  ~Model() {
    try {
      clean_up();
    } catch (...) {
    }
  }

  void init() {
    telnet->init();
    proxy_manager->init();

    auto telnet_loop = [this]() { telnet->start(keep_running); };
    telnet_ft = async(launch::async, telnet_loop);

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

  bool react(EventUserInput* event) {
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
        proxy_manager->discover_proxies();
      } else if (cursor_line == num_options) {
        *keep_running = 0;
      } else {
        for (auto& proxy : proxies) {
          proxy.second->active = false;
        }
        int proxy_index = cursor_line - 2;
        auto ids = get_ordered_proxy_ids();
        auto selected_proxy_id = ids[proxy_index];
        proxies[selected_proxy_id]->active = true;
      }
    } else {
      // unrecognized input, do nothing
    }
    return true;
  }

  bool react(EventIamSent* event) {
    auto sender_id = event->sender_id;
    auto it = proxies.find(sender_id);
    if (it != proxies.end()) {
      auto proxy = it->second;
      proxy->info = *(event->iam);
    } else {
      proxies[sender_id] =
          make_shared<ProxyInfo>(*(event->iam), "", sender_id, event->timestamp, false);
    }
    proxies[sender_id]->last_contact = event->timestamp;
    return true;
  }

  bool react(EventMetaSent* event) {
    auto sender_id = event->sender_id;
    auto it = proxies.find(sender_id);
    if (it != proxies.end()) {
      auto proxy = it->second;
      proxy->meta = *(event->meta);
      return true;
    }
    return false;
  }

  bool react(EventAudioSent* event) {
    auto sender_id = event->sender_id;
    auto it = proxies.find(sender_id);
    if (it != proxies.end() && it->second->active) {
      for (size_t i = 0; i < event->length; i++) {
        cout << event->audio[i];
      }
    }
    return false;
  }

  bool react(EventProxyManagerCrashed* event) {
    *keep_running = 0;
    rethrow_exception(event->exc);
    return false;
  }

  bool remove_inactive_proxies() {
    i64 disconnect_after_ms = proxy_timeout * 1000;
    i64 current_time = now();
    bool proxies_were_removed = false;
    auto it = proxies.begin();
    while (it != proxies.end()) {
      if (current_time - it->second->last_contact > disconnect_after_ms) {
        it = proxies.erase(it);
        proxies_were_removed = true;
      } else {
        it++;
      }
    }
    return proxies_were_removed;
  }

  void render() {
    cursor_line = max(1, cursor_line);
    cursor_line = min(get_num_menu_options(), cursor_line);
    vector<shared_ptr<ProxyInfo>> proxy_info_vector;
    for (auto id : get_ordered_proxy_ids()) {
      proxy_info_vector.push_back(proxies[id]);
    }
    string ui = generate_ui(proxy_info_vector);
    telnet->render(ui, cursor_line);
  }

  void process_event_from_queue() {
    if (event_queue.empty()) return;
    auto event = event_queue.front();
    event_queue.pop();
    bool should_render = false;
    if (auto ev = dynamic_cast<EventUserInput*>(event.get())) {
      should_render = react(ev);
    } else if (auto ev = dynamic_cast<EventIamSent*>(event.get())) {
      should_render = react(ev);
    } else if (auto ev = dynamic_cast<EventAudioSent*>(event.get())) {
      should_render = react(ev);
    } else if (auto ev = dynamic_cast<EventMetaSent*>(event.get())) {
      should_render = react(ev);
    } else if (auto ev = dynamic_cast<EventProxyManagerCrashed*>(event.get())) {
      should_render = react(ev);
    };
    if (should_render) render();
  }

  void start() {
    while (*keep_running) {
      unique_lock<mutex> lock(lock_mutex);
      cv.wait_for(lock, 100ms, []() { return true; });
      while (!event_queue.empty()) {
        process_event_from_queue();
      }
      if (remove_inactive_proxies()) render();
    }
  }
};

#endif
