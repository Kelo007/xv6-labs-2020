#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
output(int num) {
  static char buf[32];
  static const char * str = "0123456789";
  int size = 0;
  while (num) {
    buf[size++] = *(str + num % 10);
    num /= 10;
  }
  char tmp;
  for (int i = 0; i < size / 2; ++i) {
    tmp = buf[i];
    buf[i] = buf[size - i - 1];
    buf[size - i - 1] = tmp;
  }
  write(1, buf, size);
}

void MyFork(int *pre)
{
  int pid = fork();
  if (pid != 0) {
    close(pre[0]);
    return;
  }
  close(pre[1]);
  int prime = 0, n;
  char buf[4];
  int nxt[2];
  int flag = 0;
  while ((n = read(pre[0], buf, 4)) == 4) {
    int transfer = *(int *) buf;
    if (prime == 0) {
      prime = transfer;
      write(1, "prime ", 6);
      output(prime);
      write(1, "\n", 1);
    } else if (transfer % prime != 0) {
      if (flag == 0) {
        pipe(nxt);
        MyFork(nxt);
        flag = 1;
      }
      write(nxt[1], (char *) &transfer, 4);
    }
  }
  close(pre[0]);
  close(nxt[1]);
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  int root[2];
  pipe(root);
  MyFork(root);
  for (int i = 2; i <= 35; ++i) {
    write(root[1], (char *) &i, 4);
  }
  close(root[1]);
  wait(0);
  exit(0);
}
