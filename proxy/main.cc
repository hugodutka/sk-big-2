#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include "broadcaster.hh"
#include "cmd.hh"
#include "icy.hh"

using namespace std;

volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) { keep_running = 0; }

int main(int argc, char** argv) {
  try {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    CmdArgs cmd;
    try {
      cmd.parse(argc, argv);
    } catch (exception& e) {
      cerr << "Failed to parse command line arguments. Reason: " << e.what() << endl;
      cerr << "Usage: " << argv[0] << " -h host -r resource -p port [-m yes|no] [-t timeout]"
           << endl;
      return 1;
    }

    bool broadcast_udp = true;
    shared_ptr<Broadcaster> broadcaster;
    if (broadcast_udp) {
      broadcaster = make_shared<UDPBroadcaster>(16000, "");
    } else {
      broadcaster = make_shared<StdoutBroadcaster>();
    }
    broadcaster->init();

    ICYStream stream = ICYStream(cmd.host, cmd.resource, cmd.port, cmd.timeout, cmd.meta);
    stream.open_stream();

    size_t buf_size = stream.get_chunk_size();
    shared_ptr<u8[]> buf(new u8[buf_size]);

    while (keep_running) {
      try {
        ICYPart part = stream.read_chunk(buf.get());
        broadcaster->broadcast(part, buf.get());
      } catch (exception& e) {
        if (keep_running) {
          throw e;
        } else {
          // if the program should end anyway, ignore the exception
          break;
        }
      }
    }

    stream.close_stream();
    broadcaster->clean_up();
    return 0;
  } catch (exception& e) {
    cerr << "An error occurred:" << endl;
    cerr << e.what() << endl;
    cerr << "Quitting." << endl;
    return 1;
  } catch (...) {
    cerr << "An unexpected error occurred. Quitting." << endl;
    return 1;
  }
}
