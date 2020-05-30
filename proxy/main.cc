#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include "broadcaster.hh"
#include "icy.hh"

using namespace std;

volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) { keep_running = 0; }

int main() {
  try {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    shared_ptr<Broadcaster> broadcaster = make_shared<StdoutBroadcaster>();
    ICYStream stream = ICYStream("waw02-03.ic.smcdn.pl", "/t050-1.mp3", 8000, 5, true);
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
