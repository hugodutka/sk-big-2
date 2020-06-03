#ifndef CMD_HH
#define CMD_HH

#include <exception>
#include <stdexcept>
#include <string>
#include "types.hh"

using namespace std;

constexpr u32 MAX_PORT = 65535;

struct CmdArgs {
  string proxy_host;
  u32 proxy_port;
  u32 tcp_port;
  u32 timeout;

  void parse(int argc, char** argv) {
    if (argc % 2 != 1) throw runtime_error("wrong number of parameters");

    bool proxy_host_set = false;
    bool proxy_port_set = false;
    bool tcp_port_set = false;
    bool timeout_set = false;

    for (int i = 1; i < argc; i += 2) {
      string flag(argv[i]);
      string value(argv[i + 1]);

      if (flag == "-H") {
        if (proxy_host_set) throw runtime_error("duplicate host flag");
        proxy_host_set = true;
        proxy_host = value;
      } else if (flag == "-P") {
        if (proxy_port_set) throw runtime_error("duplicate proxy port flag");
        proxy_port_set = true;
        proxy_port = stoul(value);
        if (proxy_port > MAX_PORT) throw runtime_error("proxy port too high");
      } else if (flag == "-p") {
        if (tcp_port_set) throw runtime_error("duplicate tcp port flag");
        tcp_port_set = true;
        tcp_port = stoul(value);
        if (tcp_port > MAX_PORT) throw runtime_error("tcp port too high");
      } else if (flag == "-T") {
        if (timeout_set) throw runtime_error("duplicate timeout flag");
        timeout_set = true;
        timeout = stoul(value);
        if (timeout == 0) throw runtime_error("invalid timeout value");
      } else {
        throw runtime_error("unexpected flag: " + flag);
      }
    }

    if (!(proxy_host_set && proxy_port_set && tcp_port_set)) {
      throw runtime_error("some required flags were not set");
    }

    timeout = timeout_set ? timeout : 5;
  }
};

#endif
