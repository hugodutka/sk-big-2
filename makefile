TARGETS = radio-proxy radio-client

all: $(TARGETS)

radio-proxy:
	g++ -std=c++17 -Wall -Wextra -O2 proxy/main.cc -o radio-proxy -lpthread

radio-client:
	g++ -std=c++17 -Wall -Wextra -O2 client/main.cc -o radio-client -lpthread

clean:
	rm -f $(TARGETS) 
