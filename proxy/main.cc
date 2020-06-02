#include <csignal>
#include <ctime>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "broadcaster.hh"
#include "cmd.hh"
#include "icy.hh"

using namespace std;

volatile sig_atomic_t keep_running = 1;

int main(int argc, char** argv) {
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
  auto signal_handler = [&sigset]() {
    timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100000000;  // 100 ms
    while (keep_running) {
      if (sigtimedwait(&sigset, nullptr, &timeout) != -1) keep_running = 0;
    };
  };
  auto ft_signal_handler = async(launch::async, signal_handler);

  try {
    CmdArgs cmd;
    try {
      cmd.parse(argc, argv);
    } catch (exception& e) {
      cerr << "Failed to parse command line arguments. Reason: " << e.what() << endl;
      cerr << "Usage: " << argv[0] << " -h host -r resource -p port [-m yes|no] [-t timeout]"
           << " [-P port] [-B multi] [-T timeout]" << endl;
      return 1;
    }

    shared_ptr<Broadcaster> broadcaster;
    if (cmd.udp_port != -1) {
      string radio_info = cmd.host + ":" + to_string(cmd.port) + cmd.resource;
      broadcaster =
          make_shared<UDPBroadcaster>(cmd.udp_port, cmd.multi, radio_info, cmd.udp_timeout);
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
          throw;
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
    keep_running = 0;
    return 1;
  } catch (...) {
    cerr << "An unexpected error occurred. Quitting." << endl;
    keep_running = 0;
    return 1;
  }
}
