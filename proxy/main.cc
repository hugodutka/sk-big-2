#include <assert.h>
#include <iostream>
#include <string>
#include "icy.cc"

using namespace std;

int main() {
  ICYStream stream = ICYStream("waw02-03.ic.smcdn.pl", "/t050-1.mp3", 8000, 5, true);
  stream.test();
}
