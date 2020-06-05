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
  shared_ptr<ProxyClient> proxy_client;
  deque<u8> input_buf;
  queue<shared_ptr<Event>> event_queue;
  mutex lock_mutex;
  condition_variable cv;
  atomic<bool>* keep_running;
  future<void> telnet_ft;
  future<void> proxy_client_ft;

  int cursor_line;
  unordered_map<u64, shared_ptr<ProxyInfo>> proxies;
  i64 last_keepalive;

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
        atomic<bool>* keep_running)
      : proxy_timeout(proxy_timeout), keep_running(keep_running) {
    auto f_notify = [this](auto e) { notify(e); };

    telnet = make_shared<TelnetServer>(telnet_port, f_notify);
    proxy_client = make_shared<ProxyClient>(proxy_host, proxy_port, f_notify);
    last_keepalive = now();
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
    proxy_client->init();

    auto telnet_loop = [this]() { telnet->start(keep_running); };
    telnet_ft = async(launch::async, telnet_loop);

    auto proxy_client_loop = [this]() { proxy_client->start(keep_running); };
    proxy_client_ft = async(launch::async, proxy_client_loop);
  }

  void clean_up() {
    *keep_running = 0;
    telnet_ft.get();
    proxy_client_ft.get();
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
        proxy_client->discover_proxies();
      } else if (cursor_line == num_options) {
        *keep_running = 0;
      } else {
        int proxy_index = cursor_line - 2;
        auto selected_proxy_id = get_ordered_proxy_ids()[proxy_index];
        auto selected_proxy = proxies[selected_proxy_id];
        selected_proxy->active = !selected_proxy->active;
        for (auto& pair : proxies) {
          auto proxy = pair.second;
          if (proxy->id != selected_proxy_id) proxy->active = false;
        }
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
      proxies[sender_id] = make_shared<ProxyInfo>(*(event->iam), "", sender_id, event->timestamp,
                                                  false, *(event->sender));
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
      proxies[sender_id]->last_contact = event->timestamp;
      return true;
    }
    return false;
  }

  bool react(EventAudioSent* event) {
    auto sender_id = event->sender_id;
    auto it = proxies.find(sender_id);
    if (it != proxies.end()) {
      it->second->last_contact = event->timestamp;
      if (it->second->active) {
        for (size_t i = 0; i < event->length; i++) {
          cout << event->audio[i];
        }
      }
    }
    return false;
  }

  bool react(EventProxyClientCrashed* event) {
    *keep_running = 0;
    rethrow_exception(event->exc);
    return false;
  }

  bool react(EventTelnetServerCrashed* event) {
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

  void send_keepalive() {
    i64 current_time = now();
    if (current_time - last_keepalive >= 3500) {
      for (auto& pair : proxies) {
        auto proxy = pair.second;
        proxy_client->send_keepalive(proxy->addr);
      }
      last_keepalive = current_time;
    }
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
    } else if (auto ev = dynamic_cast<EventProxyClientCrashed*>(event.get())) {
      should_render = react(ev);
    } else if (auto ev = dynamic_cast<EventTelnetServerCrashed*>(event.get())) {
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
      send_keepalive();
    }
  }
};

#endif
