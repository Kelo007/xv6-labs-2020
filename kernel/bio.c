// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// a prime number
#define NTABLE (13)

struct {
  struct spinlock lock;
  struct buf head;
} hashtable[NTABLE];

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NTABLE; ++i)
    initlock(&hashtable[i].lock, "bcache.hashtable");

  for (b = bcache.buf; b < bcache.buf+NBUF; ++b) {
    int nblock = (b - bcache.buf) % NTABLE;
    b->next = hashtable[nblock].head.next;
    hashtable[nblock].head.next = b;
    b->nblock = nblock;
  }
}

// hold lock before calling
static struct buf*
findLRU(int nblock) {
  struct buf *b, *lru = 0;
  // the initial value of time is large
  uint time = 0x3FFFFFFF;
  for (b = hashtable[nblock].head.next; b; b = b->next) {
    if (b->refcnt == 0 && b->timestamp <= time) {
      time = b->timestamp;
      lru = b;
    }
  }
  return lru;
}
// hold lock before calling
static void
remove(int nblock, struct buf *p) {
  struct buf *b;
  for (b = &hashtable[nblock].head; b->next != p; b = b->next);
  b->next = p->next;
  p->next = 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // Is the block already cached?
  int nblock = blockno % NTABLE;
  acquire(&hashtable[nblock].lock);
  for (b = hashtable[nblock].head.next; b; b = b->next)
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&hashtable[nblock].lock);
      acquiresleep(&b->lock);
      return b;
    }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer implempted by timestamp.
  b = findLRU(nblock);
  if (b) {
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&hashtable[nblock].lock);
    acquiresleep(&b->lock);
    return b;
  }
  // find in other tales
  // holding two blocks in order
  // it's possible to have deadlock here, the best way to avoid deadlock is acquire
  // lock in order, and check whether the block has already benn in cache,
  // but it may lose performance.
  for (int pblock = (nblock + 1) % NTABLE; pblock != nblock; pblock = (pblock + 1) % NTABLE) if (pblock != nblock) {
    acquire(&hashtable[pblock].lock);
    b = findLRU(pblock);
    if (b) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->nblock = nblock;
      remove(pblock, b);
      b->next = hashtable[nblock].head.next;
      hashtable[nblock].head.next = b;
      release(&hashtable[pblock].lock);
      release(&hashtable[nblock].lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&hashtable[pblock].lock);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  int nblock = b->nblock;
  acquire(&hashtable[nblock].lock);
  b->refcnt--;
  if (b->refcnt == 0)
    b->timestamp = ticks;
  release(&hashtable[nblock].lock);
}

void
bpin(struct buf *b) {
  acquire(&hashtable[b->nblock].lock);
  b->refcnt++;
  release(&hashtable[b->nblock].lock);
}

void
bunpin(struct buf *b) {
  acquire(&hashtable[b->nblock].lock);
  b->refcnt--;
  release(&hashtable[b->nblock].lock);
}


