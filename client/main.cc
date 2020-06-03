#include <csignal>
#include <ctime>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "cmd.hh"

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
      cerr << "Failed to parse command line arguments. Reason: " << e.what() << endl << endl;
      cerr << "Usage: " << argv[0] << " -H host -P port -p port [-T timeout]" << endl;
      return 1;
    }
    u8 dupa[] = {1, 2, 3, 4, 5};
    cout << sizeof(dupa) << endl;

    keep_running = 0;
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
