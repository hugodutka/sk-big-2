#include <exception>
#include <string>

using namespace std;

class Exception : public exception {
  const char* msg;
  virtual const char* what() const throw() { return msg; }

 public:
  Exception(const char* msg) : msg(msg) {}
};
