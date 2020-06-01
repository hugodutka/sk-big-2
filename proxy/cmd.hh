#ifndef CMD_HH
#define CMD_HH

#include <exception>
#include <stdexcept>
#include <string>
#include "types.hh"

using namespace std;

constexpr u32 MAX_PORT = 65535;

struct CmdArgs {
  string host;
  string resource;
  u32 port;
  bool meta;
  u32 timeout;

  i32 udp_port;
  string multi;
  u32 udp_timeout;

  void parse(int argc, char** argv) {
    if (argc % 2 != 1) throw runtime_error("wrong number of parameters");

    bool host_set = false;
    bool resource_set = false;
    bool port_set = false;
    bool meta_set = false;
    bool timeout_set = false;
    bool udp_port_set = false;
    bool multi_set = false;
    bool udp_timeout_set = false;

    for (int i = 1; i < argc; i += 2) {
      string flag(argv[i]);
      string value(argv[i + 1]);

      if (flag == "-h") {
        if (host_set) throw runtime_error("duplicate host flag");
        host_set = true;
        host = value;
      } else if (flag == "-r") {
        if (resource_set) throw runtime_error("duplicate resource flag");
        resource_set = true;
        resource = value;
      } else if (flag == "-p") {
        if (port_set) throw runtime_error("duplicate port flag");
        port_set = true;
        port = stoul(value);
        if (port > MAX_PORT) throw runtime_error("port too high");
      } else if (flag == "-m") {
        if (meta_set) throw runtime_error("duplicate meta flag");
        if (value == "yes") {
          meta = true;
        } else if (value == "no") {
          meta = false;
        } else {
          throw runtime_error("unexpected value for -m: " + value);
        }
        meta_set = true;
      } else if (flag == "-t") {
        if (timeout_set) throw runtime_error("duplicate timeout flag");
        timeout_set = true;
        timeout = stoul(value);
        if (timeout == 0) throw runtime_error("timeout cannot be set to 0");
      } else if (flag == "-P") {
        if (udp_port_set) throw runtime_error("duplicate udp port flag");
        udp_port_set = true;
        udp_port = stoul(value);
        if (udp_port > MAX_PORT) throw runtime_error("udp port too high");
      } else if (flag == "-B") {
        if (multi_set) throw runtime_error("duplicate multi flag");
        multi_set = true;
        multi = value;
      } else if (flag == "-T") {
        if (udp_timeout_set) throw runtime_error("duplicate udp timeout flag");
        udp_timeout_set = true;
        udp_timeout = stoul(value);
        if (udp_timeout == 0) throw runtime_error("udp timeout cannot be set to 0");
      } else {
        throw runtime_error("unexpected flag: " + flag);
      }
    }

    if (!(host_set && resource_set && port_set)) {
      throw runtime_error("some required flags were not set");
    }

    meta = meta_set ? meta : false;
    timeout = timeout_set ? timeout : 5;
    udp_port = udp_port_set ? udp_port : -1;
    multi = multi_set ? multi : "";
    udp_timeout = udp_timeout_set ? udp_timeout : 5;
  }
};

#endif
