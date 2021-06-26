#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"
char buf[512];

char*
getline(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    if(c == '\n' || c == '\r')
      break;
    buf[i++] = c;
  }
  buf[i] = '\0';
  return buf;
}
int
main(int argc, char *argv[])
{
  char *new_argv[MAXARG];
  memcpy(new_argv, argv + 1, sizeof(char *) * (argc - 1));
  char **p = new_argv + argc - 1;
  while (1) {
    getline(buf, sizeof(buf));
    int len = strlen(buf);
    if (len == 0) {
      break;
    }
    *p = buf;
    *(p + 1) = 0;
    if (fork() == 0) {
      exec(argv[1], new_argv);
      exit(1);
    }
    wait(0);
  }
  
  exit(0);
}
