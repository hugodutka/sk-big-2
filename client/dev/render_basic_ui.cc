int sock = socket(PF_INET, SOCK_STREAM, 0);
if (sock < 0) {
  throw std::runtime_error("sock");
}
struct sockaddr_in server_address;
server_address.sin_family = AF_INET;                 // IPv4
server_address.sin_addr.s_addr = htonl(INADDR_ANY);  // listening on all interfaces
server_address.sin_port = htons(10004);              // listening on port PORT_NUM

if (bind(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
  throw std::runtime_error("bind");
}
if (listen(sock, 5) < 0) {
  throw std::runtime_error("listen");
}

struct sockaddr_in client_address;
socklen_t client_address_len;
client_address_len = sizeof(client_address);

std::cout << "sock: " << sock << std::endl;
int msg_sock = accept(sock, (struct sockaddr*)&client_address, &client_address_len);
if (msg_sock < 0) {
  throw std::runtime_error("accept");
}
std::cout << "msg_sock: " << msg_sock << std::endl;

char buffer[20];
size_t snd_len;

unsigned char do_linemode[3] = {255, 253, 34};
int len = 3;
write(msg_sock, do_linemode, len);
len = 7;
unsigned char linemode_options[7] = {255, 250, 34, 1, 0, 255, 240};
write(msg_sock, linemode_options, len);

unsigned char will_echo[3] = {255, 251, 1};
len = 3;
write(msg_sock, will_echo, len);

sleep(1);
char buff[100];
read(msg_sock, buff, 100);

unsigned char invisible_cursor[] = {27, '[', '?', '2', '5', 'l'};
write(msg_sock, invisible_cursor, sizeof(invisible_cursor));

unsigned char set_cursor_position[] = {27, '[', 's'};
unsigned char reset_cursor[] = {27, '[', 'u'};
//        unsigned char clear[] = {27, '[', 'u', 27, '[', '0', 'J', 27, '[', 's'};
unsigned char clear[] = {27, '[', 'H', 27, '[', '0', 'J'};

char* interface[5];
interface[0] = "opcja 1";
interface[1] = "opcja 2";
interface[2] = "opcja 3";
interface[3] = "opcja 4";
interface[4] = "opcja 5";

char nl[2] = {10, 13};
char* star = " * ";

int selected = 0;
size_t len_t;
do {
  write(msg_sock, clear, sizeof(clear));
  for (int i = 0; i < 5; i++) {
    write(msg_sock, interface[i], sizeof(interface[i]));
    if (i == selected) write(msg_sock, star, 3);
    write(msg_sock, nl, 2);
  }

  len_t = read(msg_sock, buffer, sizeof(buffer));

  for (int i = 0; i < len_t; i++) {
    printf("%d ", buffer[i]);
  }
  printf("\n");

  if (len_t == 3 && buffer[0] == 27 && buffer[1] == 91) {
    if (buffer[2] == 65) {
      selected = (4 + selected) % 5;
    }
    if (buffer[2] == 66) {
      selected = (++selected) % 5;
    }
  }

} while (len_t > 0);

close(sock);
