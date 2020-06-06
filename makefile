CXX = g++
CPPFLAGS = -std=c++17 -Wall -Wextra -O2 -lpthread
TARGETS = radio-proxy radio-client

all: $(TARGETS)

radio-proxy:
	$(CXX) $(CPPFLAGS) proxy/main.cc -o radio-proxy

radio-client:
	$(CXX) $(CPPFLAGS) client/main.cc -o radio-client

clean:
	rm -f $(TARGETS) 
