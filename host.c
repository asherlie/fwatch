#include "host.h"
#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

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

void p_fwpac(struct fwpa_cont* fp){
      for(int i = 0;i  < fp->sz; ++i){
            puts(fp->fwpa_p[i]->fn);
      }
}
void remove_fwpa_cont(struct fwpa_cont* fwpac, struct fwp_arg* node){
      pthread_mutex_lock(&fwpac->fwpa_lock);
      for(int i = 0; i < fwpac->sz; ++i){
            if(fwpac->fwpa_p[i] == node){
                  memmove(fwpac->fwpa_p+i, fwpac->fwpa_p+i+1, sizeof(struct fwp_arg*)*fwpac->sz-i-1);
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

_Bool sock_exists(){
      struct stat st;
      return stat(SOCK_FILE, &st) != -1;
}

int wait_conn(char* recp){
      /* creating new process and exiting parent */
      if(fork() != 0)return 0;
      int host_sock = socket(AF_UNIX, SOCK_STREAM, 0);

      struct sockaddr_un s_inf;
      s_inf.sun_family = AF_UNIX;
      strcpy(s_inf.sun_path, SOCK_FILE);

      if(sock_exists())remove(SOCK_FILE);

      bind(host_sock, (struct sockaddr*)&s_inf, sizeof(struct sockaddr_un));
      listen(host_sock, 0);

      int cli_sock, msg_type, msglen;
      while(1){
            cli_sock = accept(host_sock, NULL, NULL);
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
                        close(host_sock);
                        remove(SOCK_FILE);
                        exit(EXIT_SUCCESS);
                        break;
            }
      }
      return 0;
}

