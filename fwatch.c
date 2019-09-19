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

void fwatch(char* fn){
      int p_s = fsum(fn), s;
      s = p_s;
      while(s == p_s){
            usleep(10000);
            p_s = s;
            s = fsum(fn);
      }
}

void mail_file(char* fn, char* recp){
      char cmd[150] = "mail -s [AUTO_NOTE_UPDATE] ";
      stpcpy(stpcpy(stpcpy(cmd+strlen(cmd), recp), " < "), fn);
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
      if(fork() != 0)return 0;
      printf("keeping an eye on file: %s\nto stop, enter kill -9 %i\n", dir_p, getpid());
      while(1){
            fwatch(dir_p);
            mail_file(dir_p, b[2]);
      }
      return 0;
}
