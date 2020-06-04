#ifndef TELNET_HH
#define TELNET_HH

#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include "types.hh"

using namespace std;

class TelnetServer {
  u16 port;

  conn_t sock;
  sockaddr_in client_address;
  socklen_t client_address_len;
  conn_t client_sock;

 private:
  void setup_connection() {
    if (sock >= 0) close(sock);
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
      throw runtime_error("socket failed");
    }
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(sock, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
      throw runtime_error("bind failed");
    }
  }

  void send_msg(const u8* msg, size_t size) {
    size_t sent_total = 0;
    ssize_t sent = 0;

    while (sent_total < size) {
      sent = write(client_sock, msg, size - sent_total);
      if (sent <= 0) throw runtime_error("write failed");
      sent_total += sent;
    }
  }

  void set_display_options() {
    static u8 do_linemode[] = {255, 253, 34};
    static u8 linemode_options[] = {255, 250, 34, 1, 0, 255, 240};
    static u8 will_echo[] = {255, 251, 1};

    send_msg(do_linemode, sizeof(do_linemode));
    send_msg(linemode_options, sizeof(linemode_options));
    send_msg(will_echo, sizeof(will_echo));
  }

  void clear_screen() {
    static u8 clear[] = {27, '[', 'H', 27, '[', '2', 'J'};
    send_msg(clear, sizeof(clear));
  }

  // Indexing starts at 1.
  void set_cursor_pos(u32 row) {
    stringstream builder;
    builder << (char)27 << "[" << to_string(row) << ";0H";
    string command = builder.str();
    send_msg((u8*)(command.c_str()), command.size());
  }

  void accept_new_connection() {
    if (client_sock >= 0) close(client_sock);

    if (listen(sock, 1) < 0) {
      throw runtime_error("listen failed");
    }
    client_address_len = sizeof(client_address);
    client_sock = accept(sock, (struct sockaddr*)&client_address, &client_address_len);
    if (client_sock < 0) {
      throw std::runtime_error("accept failed");
    }

    set_display_options();
  }

  tuple<u8, bool> read_input() {
    u8 input;
    fd_set set;
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100 ms

    FD_ZERO(&set);
    FD_SET(client_sock, &set);

    int status = select(client_sock + 1, &set, NULL, NULL, &timeout);
    if (status == -1) {
      throw runtime_error("select failed");
    } else if (status == 0) {
      // timeout occured
      return {0, false};
    } else {
      if (read(client_sock, &input, 1) != 1) throw runtime_error("read failed");
      return {input, true};
    }
  }

 public:
  TelnetServer(u16 port) : port(port) {
    sock = -1;
    client_sock = -1;
  }

  ~TelnetServer() {
    try {
      clean_up();
    } catch (...) {
      // ignore errors in destructor
    }
  }

  void init() { setup_connection(); }

  void clean_up() {
    bool sock_failed = false;
    bool client_sock_failed = false;

    if (sock >= 0) {
      sock_failed = close(sock) != 0;
    }
    if (client_sock >= 0) {
      client_sock_failed = close(sock) != 0;
    }

    if (sock_failed) throw runtime_error("sock close failed");
    if (client_sock_failed) throw runtime_error("client sock close failed");
  }

  void start(volatile sig_atomic_t* keep_running, function<void(u8)> notify) {
    try {
      accept_new_connection();
      while (*keep_running) {
        try {
          const auto& [in, read_succeeded] = read_input();
          if (read_succeeded) notify(in);
        } catch (...) {
          accept_new_connection();
        }
      }
    } catch (exception& e) {
      cerr << "telnet start failed:" << endl;
      cerr << e.what() << endl;
      throw;
    } catch (...) {
      cerr << "telnet start failed for an unknown reason" << endl;
      throw;
    }
  }

  void render(const string& text, u32 cursor_pos) {
    clear_screen();
    send_msg((const u8*)(text.c_str()), text.size());
    set_cursor_pos(cursor_pos);
  }
};

#endif
