#include <iostream>
#include <memory>
#include <string>
#include "broadcaster.hh"
#include "icy.hh"

using namespace std;

int main() {
  shared_ptr<Broadcaster> broadcaster = make_shared<StdoutBroadcaster>();
  ICYStream stream = ICYStream("waw02-03.ic.smcdn.pl", "/t050-1.mp3", 8000, 5, true);
  stream.open_stream();

  size_t buf_size = stream.get_chunk_size();
  shared_ptr<u8[]> buf(new u8[buf_size]);

  for (u64 i = 0; i < 10; i++) {
    ICYPart part = stream.read_chunk(buf.get());
    broadcaster->broadcast(part, buf.get());
  }

  stream.close_stream();
}
