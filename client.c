#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>

#include "shared.h"

int cli_connect(){
      int conn_sock = socket(AF_UNIX, SOCK_STREAM, 0);
      struct sockaddr_un host_addr;
      host_addr.sun_family = AF_UNIX;
      strcpy(host_addr.sun_path, SOCK_FILE);
      if(connect(conn_sock, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_un)) == -1){
            /*close(conn_sock);*/
            return -1;
      }
      return conn_sock;
}

/* TODO: allow adding directories */
void add_file(char* fname){
      int host_sock = cli_connect();
      int msglen = strlen(fname);
      send_header(host_sock, MSG_ADD, sizeof(char)*msglen);
      send(host_sock, fname, sizeof(char)*msglen, 0);
}

void rm_file(int f_ind){
      int host_sock = cli_connect();
      send_header(host_sock, MSG_REM, 0);
      send(host_sock, &f_ind, sizeof(int), 0);
}

_Bool list_files(){
      int host_sock = cli_connect(), mtype, len;
      send_header(host_sock, MSG_LST_REQ, 0);

      read_header(host_sock, &mtype, &len);
      if(mtype != MSG_LST_UPD)return 0;

      /* read fnames one at a time */
      
      char fn[100];
      for(int i = 0; i < len; ++i){
            memset(fn, 0, sizeof(char)*100);
            read(host_sock, fn, sizeof(char)*100);
            printf("%i): \"%s\"\n", i, fn);
      }
      return 0;
}

void quit(){
      int host_sock = cli_connect();
      send_header(host_sock, MSG_QUIT, 0);
}
