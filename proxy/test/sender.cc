#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#define TTL_VALUE 4
#define REPEAT_COUNT 3
#define SLEEP_TIME 1

using namespace std;

int main(int argc, char *argv[]) {
  /* argumenty wywołania programu */
  char *remote_dotted_address;
  in_port_t remote_port;

  /* zmienne i struktury opisujące gniazda */
  int sock, optval;
  struct sockaddr_in remote_address;

  /* zmienne obsługujące komunikację */
  size_t length;
  time_t time_buffer;
  int i;

  /* parsowanie argumentów programu */
  if (argc != 3) throw runtime_error("Usage: " + string(argv[0]) + " remote_address remote_port");
  remote_dotted_address = argv[1];
  remote_port = (in_port_t)atoi(argv[2]);

  /* otwarcie gniazda */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) throw runtime_error("socket");

  /* uaktywnienie rozgłaszania (ang. broadcast) */
  optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof optval) < 0)
    throw runtime_error("setsockopt broadcast");

  /* ustawienie TTL dla datagramów rozsyłanych do grupy */
  optval = TTL_VALUE;
  if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&optval, sizeof optval) < 0)
    throw runtime_error("setsockopt multicast ttl");

  /* ustawienie adresu i portu odbiorcy */
  remote_address.sin_family = AF_INET;
  remote_address.sin_port = htons(remote_port);
  if (inet_aton(remote_dotted_address, &remote_address.sin_addr) == 0) {
    fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
    exit(EXIT_FAILURE);
  }

  /* związanie z gniazdem adresu i portu odbiorcy, aby móc użyć write zamiast sendto */
  if (connect(sock, (struct sockaddr *)&remote_address, sizeof remote_address) < 0)
    throw runtime_error("connect");

  uint8_t buf[1024];
  ((uint16_t *)(&buf))[0] = htons(1);
  ((uint16_t *)(&buf))[1] = htons(0);
  length = 4;
  if (write(sock, &buf, length) != length) throw runtime_error("write");

  ssize_t read_num = read(sock, &buf, 1024);
  if (read_num < 4) throw runtime_error("read: " + to_string(read_num));
  cerr << "read msg type: " << ((uint16_t *)(&buf))[0] << endl;
  cerr << "read msg length: " << ((uint16_t *)(&buf))[1] << endl;
  cerr << string(begin(buf) + 4, begin(buf) + 4 + ((uint16_t *)(&buf))[1]) << endl;

  /* koniec */
  close(sock);
  exit(EXIT_SUCCESS);
}
