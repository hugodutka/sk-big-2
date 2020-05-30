#ifndef CMD_HH
#define CMD_HH

#include <exception>
#include <stdexcept>
#include <string>
#include "types.hh"

using namespace std;

struct CmdArgs {
  string host;
  string resource;
  u32 port;
  bool meta;
  u32 timeout;

  void parse(int argc, char** argv) {
    if (argc % 2 != 1) throw runtime_error("wrong number of parameters");

    bool host_set = false;
    bool resource_set = false;
    bool port_set = false;
    bool meta_set = false;
    bool timeout_set = false;

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
      } else {
        throw runtime_error("unexpected flag: " + flag);
      }
    }

    if (!(host_set && resource_set && port_set)) {
      throw runtime_error("some required flags were not set");
    }

    meta = meta_set ? meta : false;
    timeout = timeout_set ? timeout : 5;
  }
};

#endif
