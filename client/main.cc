#include <csignal>
#include <ctime>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "cmd.hh"
#include "events.hh"
#include "model.hh"
#include "telnet.hh"

using namespace std;

volatile sig_atomic_t keep_running = 1;

int main(int argc, char** argv) {
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGPIPE);  // the OS sends SIGPIPE if write is performed after telnet closed
  pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
  auto signal_handler = [&sigset]() {
    siginfo_t info;
    timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100000000;  // 100 ms
    while (keep_running) {
      if (sigtimedwait(&sigset, &info, &timeout) != -1 && info.si_signo != SIGPIPE) {
        keep_running = 0;
      }
    };
  };
  auto ft_signal_handler = async(launch::async, signal_handler);

  try {
    CmdArgs cmd;
    try {
      cmd.parse(argc, argv);
    } catch (exception& e) {
      cerr << "Failed to parse command line arguments. Reason: " << e.what() << endl << endl;
      cerr << "Usage: " << argv[0] << " -H host -P port -p port [-T timeout]" << endl;
      return 1;
    }
    Model model(cmd.tcp_port, cmd.proxy_host, cmd.proxy_port, &keep_running);
    model.init();
    model.start();

    keep_running = 0;
    model.clean_up();
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
