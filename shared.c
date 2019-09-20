#include "shared.h"

#include <sys/socket.h>
#include <unistd.h>

void send_header(int sock, int msg, int msglen){
      send(sock, &msg, sizeof(int), 0);
      send(sock, &msglen, sizeof(int), 0);
}

void read_header(int sock, int* msg, int* msglen){
      read(sock, msg, sizeof(int));
      read(sock, msglen, sizeof(int));
}
