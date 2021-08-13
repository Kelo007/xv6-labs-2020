// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int length;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i) {
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

// it will git all available memory to the currently running CPU
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// should disable interrupt
void balance(int id) {
  if (kmem[id].freelist) return;
  struct run *r = 0, *tmp;
  int length = 0;
  for (int i = 0; i < NCPU; ++i) if (i != id) {
    acquire(&kmem[i].lock);
    if (kmem[i].freelist) {
      int times = kmem[i].length / 2;
      length = times + 1;
      tmp = kmem[i].freelist;
      while (times--)
        tmp = tmp->next;
      r = kmem[i].freelist;
      kmem[i].freelist = tmp->next;
      tmp->next = 0;
      kmem[i].length -= length;
      release(&kmem[i].lock);
      break;
    }
    release(&kmem[i].lock);
  }
  acquire(&kmem[id].lock);
  kmem[id].freelist = r;
  kmem[id].length = length;
  release(&kmem[id].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].length += 1;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  balance(id);
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  kmem[id].length -= 1;
  release(&kmem[id].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
