#include <pthread.h>

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

//void fwatch(char* fn, _Bool* run, void notif_func(char*, char*), char* nfargs[2]);
_Bool sock_exists();
void init_fwpa_cont(struct fwpa_cont* fwpac);
int wait_conn(char* recp);
