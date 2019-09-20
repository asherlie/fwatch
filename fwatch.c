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

TODO:
      fwatch should run in the background and accept connections to:
            {add, remove, list} watch files
            kill the fwatch process
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_FILE "/var/tmp/fwatch_sock.decaf"

#define MSG_ADD 0
#define MSG_REM 1

#define MSG_LST_REQ 2
#define MSG_LST_UPD 3

/* HOST */

/* fsz assumes that the fp is at the beginning of the buffer */
long fsum(char* fn){
      FILE* fp = fopen(fn, "r");
      /* check if it's UB that fopen() returns NULL
       * when file is being written to
       * 
       * if not, we can take advantage of this exclusively
       * to detect file writes
       */
      if(!fp)return -1;
      long sum = 0;
      int ch;
      while((ch = fgetc(fp)) != EOF)sum += ch;
      fclose(fp);
      return sum;
}

/* notif_func is a function pointer to the routine that should be run
 * to notify the user about file changes
 */
void fwatch(char* fn, _Bool* run, void notif_func(char*, char*), char* nfargs[2]){
      int p_s = fsum(fn), s;
      s = p_s;
      while(*run && s == p_s){
            usleep(10000);
            p_s = s;
            s = fsum(fn);
      }
      notif_func(nfargs[0], nfargs[1]);
}

void mail_file(char* fn, char* recp){
      char cmd[150] = "mail -s [AUTO_NOTE_UPDATE@";
      stpcpy(stpcpy(stpcpy(stpcpy(stpcpy(cmd+strlen(cmd), fn), "] "), recp), " < "), fn);
      /* give some time for the FP to be closed */
      usleep(10000);
      system(cmd);
}

struct fwp_arg{
      /* we can likely get rid of recp */
      char fn[100], recp[100];
      _Bool* active;
      void* notif_func;

      /* this entry is used __ */
      pthread_t pth;
};

void* fwatch_pth(void* fwpa_v){
      struct fwp_arg* fwpa = (struct fwp_arg*)fwpa_v;
      char dir[100] = {0};
      char* dir_p = (*fwpa->fn == '/') ? fwpa->fn : dir;
      char* nfargs[2] = {dir_p, fwpa->recp};
      if(*fwpa->fn != '/'){
            getcwd(dir, 100);
            sprintf(dir+strlen(dir), "/");
            strcpy(dir+strlen(dir), fwpa->fn);
      }
      while(*fwpa->active)
            fwatch(dir_p, fwpa->active, &mail_file, nfargs);
      return NULL;
}

void read_header(int sock, int* msg, int* msglen){
      read(sock, msg, sizeof(int));
      read(sock, msglen, sizeof(int));
}

/* this is used to keep track of file watches in place */
struct fwpa_cont{
      pthread_mutex_t fwpa_lock;

      struct fwp_arg** fwpa_p;
      int sz, cap;
};

void init_fwpa_cont(struct fwpa_cont* fwpac){
      pthread_mutex_init(&fwpac->fwpa_lock, NULL);

      fwpac->cap = 20;
      fwpac->sz = 0;

      fwpac->fwpa_p = malloc(sizeof(struct fwp_arg*)*fwpac->cap);
}

void insert_fwpa_cont(struct fwpa_cont* fwpac, struct fwp_arg* node){
      if(fwpac->sz == fwpac->cap){
            fwpac->cap *= 2;
            struct fwp_arg** tmp = malloc(sizeof(struct fwp_arg*)*fwpac->cap);
            memcpy(tmp, fwpac->fwpa_p, sizeof(struct fwp_arg*)*fwpac->cap);
            free(fwpac->fwpa_p);
            fwpac->fwpa_p = tmp;
      }
      fwpac->fwpa_p[fwpac->sz++] = node;
}

void remove_fwpa_cont(struct fwpa_cont* fwpac, struct fwp_arg* node){
      for(int i = 0; i < fwpac->sz; ++i){
            if(fwpac->fwpa_p[i] == node){
                  printf("shifting by %i\n", fwpac->sz-i-1);
                  memmove(fwpac->fwpa_p+i, fwpac->fwpa_p+i+1, fwpac->sz-i-1);

                  *node->active = 0;
                  pthread_join(node->pth, NULL);

                  free(node);

                  --fwpac->sz;
            }
      }
}

void send_file_inf(){
}

int wait_conn(char* recp){
      /* creating new process and exiting parent */
      /*if(fork() != 0)return 0;*/
      int host_sock = socket(AF_UNIX, SOCK_STREAM, 0);

      struct sockaddr_un s_inf;
      s_inf.sun_family = AF_UNIX;
      strcpy(s_inf.sun_path, SOCK_FILE);

      remove(SOCK_FILE);

      bind(host_sock, (struct sockaddr*)&s_inf, sizeof(struct sockaddr_un));
      listen(host_sock, 0);

      int cli_sock, msg_type, msglen;
      while(1){
            cli_sock = accept(host_sock, NULL, NULL);
            /* TODO: keep track of pthread_t, cli_sock in a structure
             * this structure should also store the fwp_arg
             * for printing purposes
             */
            /* before we can spawn the watch thread we need to receive filename
             */
            read_header(cli_sock, &msg_type, &msglen);

            switch(msg_type){
                  case MSG_ADD:{
                        struct fwp_arg* fwpa = malloc(sizeof(struct fwp_arg));

                        fwpa->active = malloc(sizeof(_Bool));
                        *fwpa->active = 1;

                        read(cli_sock, &fwpa->fn, msglen);

                        close(cli_sock);

                        pthread_t pth;
                        strcpy(fwpa->recp, recp);
                        pthread_create(&pth, NULL, &fwatch_pth, fwpa);
                        }
                  /*case MSG_LST_REQ:*/
            }
      }
      return 0;
}

/* HOST END */

/* CLIENT */

int cli_connect(){
      int conn_sock = socket(AF_UNIX, SOCK_STREAM, 0);
      struct sockaddr_un host_addr;
      host_addr.sun_family = AF_UNIX;
      strcpy(host_addr.sun_path, SOCK_FILE);
      connect(conn_sock, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_un));
      return conn_sock;
}

void send_header(int sock, int msg, int msglen){
      send(sock, &msg, sizeof(int), 0);
      send(sock, &msglen, sizeof(int), 0);
}

void add_file(char* fname){
      int host_sock = cli_connect();
      int msglen = strlen(fname);
      send_header(host_sock, MSG_ADD, sizeof(char)*msglen);
      send(host_sock, fname, sizeof(char)*msglen, 0);
}

void list_files(){
      int host_sock = cli_connect();
      send_header(host_sock, MSG_LST_REQ, 0);
}

/* CLIENT END */

int main(int a, char** b){
      if(a == 2){
            wait_conn(b[1]);
            return 0;
      }
      if(a > 2){
            switch(*b[1]){
                  case 'a':
                        add_file(b[2]);
                        break;
            }
            /* client mode */
            return 0;
      }
      /*printf("usage: %s ")*/
      return 1;
}
