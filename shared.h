#define SOCK_FILE "/var/tmp/fwatch_sock.decaf"

#define MSG_ADD 0
#define MSG_REM 1

#define MSG_LST_REQ 2
#define MSG_LST_UPD 3

#define MSG_QUIT 4

void send_header(int sock, int msg, int msglen);
void read_header(int sock, int* msg, int* msglen);
