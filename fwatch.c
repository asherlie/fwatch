#if 0

fwatch monitors files for changes
once a change is detected, an email containing the contents
of the file being monitored is sent

compilation:
      gcc fwatch.c -Wall -Wextra -Werror -Wpedantic -D_GNU_SOURCE -O3 -o fwatch
usage:
      ./fwatch [filename] [email_recipient]


TODO:
      fwatch should run in the background and accept connections to:
            {add, remove, list} watch files
            kill the fwatch process
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

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
void fwatch(char* fn, void notif_func(char*, char*), char* nfargs[2]){
      int p_s = fsum(fn), s;
      s = p_s;
      while(s == p_s){
            usleep(10000);
            p_s = s;
            s = fsum(fn);
      }
      notif_func(nfargs[0], nfargs[1]);
}

void mail_file(char* fn, char* recp){
      char cmd[150] = "mail -s [AUTO_NOTE_UPDATE] ";
      stpcpy(stpcpy(stpcpy(cmd+strlen(cmd), recp), " < "), fn);
      /* give some time for the FP to be closed */
      usleep(10000);
      system(cmd);
}

int main(int a, char** b){
      if(a < 3)return 0;
      char dir[100] = {0};
      char* dir_p = (*b[1] == '/') ? b[1] : dir;
      if(*b[1] != '/'){
            getcwd(dir, 100);
            sprintf(dir+strlen(dir), "/");
            strcpy(dir+strlen(dir), b[1]);
      }
      printf("keeping an eye on file: %s\n", dir_p);
      if(fork() != 0)return 0;
      char* notif_arg[2] = {dir_p, b[2]};
      while(1)
            fwatch(dir_p, &mail_file, notif_arg);
      return 0;
}
