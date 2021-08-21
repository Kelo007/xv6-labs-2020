#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "memlayout.h"

int vmap(pagetable_t pagetable, struct VMA *vma) {
  uint64 va = PGROUNDDOWN(vma->addr);
  uint64 end = PGROUNDUP(vma->addr + vma->length - 1);
  pte_t *pte;
  for (; va < end; va += PGSIZE) {
    pte = walk(pagetable, va, 1);
    if (pte == 0)
      panic("vmap: failed");
    // lazy allocation
    *pte = 0;
  }
  return 0;
}

int vunmap(pagetable_t pagetable, struct VMA *vma) {
  if (vma->addr % PGSIZE != 0) {
    printf("vunmap: addr is not aligned\n");
    return -1;
  }
  if (!vma->flags)
    return 0;
  uint64 va = vma->addr;
  uint64 end = va + vma->length;
  pte_t *pte;
  begin_op();
  ilock(vma->f->ip);
  for (; va < end; va += PGSIZE) {
    pte = walk(pagetable, va, 0);
    if (pte == 0)
      continue;
    if (*pte & PROT_WRITE) {
      int offset = vma->offset + (va - vma->addr);
      int n = end - va < PGSIZE ? end - va : PGSIZE;
      writei(vma->f->ip, 0, PTE2PA(*pte), offset, n);
    }
  }
  iunlock(vma->f->ip);
  end_op();
  return 0;
}
int vinstall(struct VMA *vma) {
  if (vma->length == 0) {
    printf("vinstall: install zero length vma\n");
    return 0;
  }
  if (vma->f->type != FD_INODE) {
    printf("vinstall: file type is not FD_INODE\n");
    return -1;
  }
  if (!vma->f->readable) {
    printf("vinstall: file is not readable\n");
    return -1;
  }
  if (!vma->f->writable && vma->flags && (vma->prot & PTE_W)) {
    printf("vintall: have write accsee to read only file\n");
    return -1;
  }


  struct proc* p = myproc();
  if (vma->addr == 0) {
    // lazy allocation
    vma->addr = PGROUNDUP(p->sz);
    p->sz = vma->addr + vma->length;
  }

  if (vmap(p->pagetable, vma) < 0) {
    printf("vintall: map failed\n");
    return -1;
  }

  filedup(vma->f);

  for (int i = 0; i < NVMA; ++i) {
    if (p->vma[i].length == 0) {
      p->vma[i] = *vma;
      return 0;
    }
  }
  return -1;
}

struct VMA * vget(uint64 va) {
  struct proc *p = myproc();
  struct VMA *vma;
  for (vma = p->vma; vma != p->vma + NVMA; ++vma) if (vma->length > 0) {
    if (va >= vma->addr && va < vma->addr + vma->length)
      return vma;
  }
  return 0;
}

int vuninstall(uint64 addr, int length) {
  struct VMA * vma = vget(addr);
  if (vma == 0) {
    printf("vuninstall: vget failed\n");
    return -1;
  }
  if (vma->length < length) {
    printf("vuninstall: invalid length\n");
    return -1;
  }

  struct VMA nvma = *vma;
  nvma.addr = addr;
  nvma.length = length;

  if (addr == vma->addr) {
    vma->addr += length;
    vma->length -= length;
    vma->offset += length;
  } else {
    vma->length -= length;
    nvma.offset += addr - vma->addr;
  }

  if (vunmap(myproc()->pagetable, &nvma) < 0)
    return -1;

  if (vma->length == 0)
    fileclose(vma->f);
  return 0;
}


int vfault(pagetable_t pagetable, uint64 va) {
  struct VMA * vma;
  if ((vma = vget(va)) == 0) {
    printf("vfalut: get vma failed\n");
    return -1;
  }
  va = PGROUNDDOWN(va);
  if (va < vma->addr)
    panic("vfault: va < addr");
  pte_t *pte = walk(pagetable, va, 1);
  if (!(*pte & PTE_V)) {
    int offset = vma->offset + (va - vma->addr);
    char *mem;
    if ((mem = kalloc()) == 0)
      return -1;
    ilock(vma->f->ip);
    int n = readi(vma->f->ip, 0, (uint64) mem, offset, PGSIZE);
    iunlock(vma->f->ip);
    if (n < 0) {
      printf("vfault: read failed\n");
      return -1;
    }
    memset(mem + n, 0, PGSIZE - n);
    *pte = PA2PTE(mem) | vma->prot;
  }
  return 0;
}