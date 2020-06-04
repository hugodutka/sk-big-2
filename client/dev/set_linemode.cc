unsigned char do_linemode[3] = {255, 253, 34};
int len = 3;
write(msg_sock, do_linemode, len);
len = 7;
unsigned char linemode_options[7] = {255, 250, 34, 1, 0, 255, 240};
write(msg_sock, linemode_options, len);

unsigned char will_echo[3] = {255, 251, 1};