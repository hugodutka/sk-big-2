#include <assert.h>
#include <iostream>
#include <string>
#include "icy.cc"

using namespace std;

int main() {
  ICYStream stream = ICYStream("localhost", "/", 5000, 5, true);
  stream.test();
}
