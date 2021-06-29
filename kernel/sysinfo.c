#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

extern uint64 kfreespace(void);
extern uint64 proccount(void);

uint64
sys_sysinfo(void) {
  uint64 addr;

  if(argaddr(0, &addr) < 0)
    return -1;
  struct sysinfo info;
  info.freemem = kfreespace();
  info.nproc = proccount();
  struct proc * p = myproc();
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
      return -1;
  return 0;
  
}