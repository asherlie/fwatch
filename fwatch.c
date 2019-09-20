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

#define MSG_QUIT 4

/* HOST */

struct fwp_arg{
      /* we can likely get rid of recp */
      char fn[100], recp[100];
      _Bool* active;
      void* notif_func;

      /* this entry is used __ */
      /* cli_sock is used in MSG_LST_UPD */
      /*int cli_sock;*/
      pthread_t pth;
};

/* this is used to keep track of file watches in place */
struct fwpa_cont{
      pthread_mutex_t fwpa_lock;

      struct fwp_arg** fwpa_p;
      int sz, cap;
};

struct fwpa_cont watched_files;

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
      strcpy(fwpa->fn, dir_p);
      while(*fwpa->active)
            fwatch(dir_p, fwpa->active, &mail_file, nfargs);
      return NULL;
}

void send_header(int sock, int msg, int msglen){
      send(sock, &msg, sizeof(int), 0);
      send(sock, &msglen, sizeof(int), 0);
}

void read_header(int sock, int* msg, int* msglen){
      read(sock, msg, sizeof(int));
      read(sock, msglen, sizeof(int));
}

void init_fwpa_cont(struct fwpa_cont* fwpac){
      pthread_mutex_init(&fwpac->fwpa_lock, NULL);

      fwpac->cap = 20;
      fwpac->sz = 0;

      fwpac->fwpa_p = malloc(sizeof(struct fwp_arg*)*fwpac->cap);
}

void insert_fwpa_cont(struct fwpa_cont* fwpac, struct fwp_arg* node){
      pthread_mutex_lock(&fwpac->fwpa_lock);
      if(fwpac->sz == fwpac->cap){
            fwpac->cap *= 2;
            struct fwp_arg** tmp = malloc(sizeof(struct fwp_arg*)*fwpac->cap);
            memcpy(tmp, fwpac->fwpa_p, sizeof(struct fwp_arg*)*fwpac->cap);
            free(fwpac->fwpa_p);
            fwpac->fwpa_p = tmp;
      }
      fwpac->fwpa_p[fwpac->sz++] = node;
      pthread_mutex_unlock(&fwpac->fwpa_lock);
}

void remove_fwpa_cont(struct fwpa_cont* fwpac, struct fwp_arg* node){
      pthread_mutex_lock(&fwpac->fwpa_lock);
      for(int i = 0; i < fwpac->sz; ++i){
            if(fwpac->fwpa_p[i] == node){
                  /*printf("khifting by %i\nmemmove(fwpa+%i,%i)", fwpac->sz-i-1, i+1, fwpac->sz-i-1);*/
                  memmove(fwpac->fwpa_p+i, fwpac->fwpa_p+i+1, sizeof(struct fwp_arg*)*fwpac->sz-i-1);

                  /*
                   *[x] -> []
                   *memmove(0, 1, 1-0-1
                   *m
                   */

                  *node->active = 0;

                  free(node);

                  --fwpac->sz;
            }
      }
      pthread_mutex_unlock(&fwpac->fwpa_lock);
}

void send_file_inf(struct fwpa_cont* fwpac, int sock){
      pthread_mutex_lock(&fwpac->fwpa_lock);
      send_header(sock, MSG_LST_UPD, fwpac->sz);
      for(int i = 0; i < fwpac->sz; ++i)
            send(sock, fwpac->fwpa_p[i]->fn, 100, 0);
      pthread_mutex_unlock(&fwpac->fwpa_lock);
}

int wait_conn(char* recp){
      /* creating new process and exiting parent */
      /*if(fork() != 0)return 0;*/
      int host_sock = socket(AF_UNIX, SOCK_STREAM, 0);

      struct sockaddr_un s_inf;
      s_inf.sun_family = AF_UNIX;
      strcpy(s_inf.sun_path, SOCK_FILE);

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

                        insert_fwpa_cont(&watched_files, fwpa);

                        fwpa->active = malloc(sizeof(_Bool));
                        *fwpa->active = 1;

                        read(cli_sock, &fwpa->fn, msglen);

                        close(cli_sock);

                        pthread_t pth;
                        strcpy(fwpa->recp, recp);

                        pthread_create(&pth, NULL, &fwatch_pth, fwpa);
                        pthread_detach(pth);

                        break;
                        }
                  case MSG_REM:{
                        int rm_ind;
                        read(cli_sock, &rm_ind, sizeof(int));
                        /* TODO: this if statement introduces possible synch issues */
                        if(rm_ind < watched_files.sz)
                              remove_fwpa_cont(&watched_files, watched_files.fwpa_p[rm_ind]);
                        }
                        break;
                  case MSG_LST_REQ:
                        send_file_inf(&watched_files, cli_sock);
                        /* TODO can we close the socket here? */
                        break;
                  case MSG_QUIT:
                        while(watched_files.sz)remove_fwpa_cont(&watched_files, *watched_files.fwpa_p);
                        free(watched_files.fwpa_p);
                        pthread_mutex_destroy(&watched_files.fwpa_lock);
                        remove(SOCK_FILE);
                        exit(EXIT_SUCCESS);
                        break;
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
      if(connect(conn_sock, (struct sockaddr*)&host_addr, sizeof(struct sockaddr_un)) == -1){
            close(conn_sock);
            return -1;
      }
      return conn_sock;
}

void add_file(char* fname){
      int host_sock = cli_connect();
      int msglen = strlen(fname);
      send_header(host_sock, MSG_ADD, sizeof(char)*msglen);
      send(host_sock, fname, sizeof(char)*msglen, 0);
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

_Bool sock_exists(){
      /* i_connect returns -1 if it cannot connect to sock */
      int sock = cli_connect();
      if(sock == -1)return 0;
      close(sock);
      return 1;
}

void quit(){
      int host_sock = cli_connect();
      send_header(host_sock, MSG_QUIT, 0);
}

/* CLIENT END */

int main(int a, char** b){
      if(a == 2){
            init_fwpa_cont(&watched_files);
            wait_conn(b[1]);
            return 0;
      }
      if(a > 2){
            switch(*b[1]){
                  case 'a':
                        add_file(b[2]);
                        break;
                  case 'l':
                        list_files();
                        break;
                  case 'q':
                        quit();
            }
            /* client mode */
            return 0;
      }
      /*printf("usage: %s ")*/
      return 1;
}
