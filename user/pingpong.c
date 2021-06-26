#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char pid2char() {
  return '0' + getpid();
}
int
main(int argc, char *argv[])
{
  int p2c[2], c2p[2];
  char from_child[2], from_parent[2];
  pipe(p2c);
  pipe(c2p);
  if (fork() == 0) {
    int n = read(p2c[0], from_parent, sizeof(from_parent));
    if (n == 1) {
      char pid = pid2char();
      write(1, &pid, 1);
      write(1, ": received ping\n", 16);
      write(c2p[1], "a", 1);
      close(p2c[1]);
    }
    exit(0);
  }
  write(p2c[1], "a", 1);
  close(p2c[1]);
  int n = read(c2p[0], from_child, sizeof(from_child));
  if (n == 1) {
    char pid = pid2char();
    write(1, &pid, 1);
    write(1, ": received pong\n", 16);
  }
  close(c2p[0]);
  close(p2c[0]);
  exit(0);
}
