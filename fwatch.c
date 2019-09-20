#if 0

fwatch monitors files for changes
once a change is detected, an email containing the contents
of the file being monitored is sent

compilation:
      gcc fwatch.c -Wall -Wextra -Werror -Wpedantic -D_GNU_SOURCE -O3 -o fwatch
usage:
      ./fwatch [filename] [email_recipient]

NOTE:
      make sure that postfix has been started before running this

#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "shared.h"
#include "host.h"
#include "client.h"

extern struct fwpa_cont watched_files;

_Bool strtoi(const char* str, int* i){
      char* res;
      unsigned int r = strtol(str, &res, 10);
      if(*res)return 0;
      if(i)*i = (int)r;
      return 1;
}

_Bool sock_open(){
      /* i_connect returns -1 if it cannot connect to sock */
      int sock = cli_connect();
      if(sock == -1)return 0;
      close(sock);
      return 1;
}

void p_usage(char* bin_name){
      printf("usage: %s <[a]dd filename> <[r]em fileno> <[l]ist> <[q]uit>\n", bin_name);
}

int main(int a, char** b){
      if(a == 2){
            switch(*b[1]){
                  case 'l':
                        list_files();
                        return 0;
                  case 'q':
                        quit();
                        return 0;
                  default:
                        if(sock_exists() && sock_open()){
                              printf("an instance of %s is already running. refusing to start\n", *b);
                              return 0;
                        }
                        init_fwpa_cont(&watched_files);
                        wait_conn(b[1]);
                        return 0;
            }
      }
      if(a > 2){
            switch(*b[1]){
                  case 'a':
                        add_file(b[2]);
                        return 0;
                  case 'r':{
                        int rm_ind;
                        if(strtoi(b[2], &rm_ind))
                        rm_file(rm_ind);
                        return 0;
                  }
            }
      }
      p_usage(*b);
      return 1;
}
