struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  int nblock;
  uint timestamp;
  struct buf *next, *prev;
  uchar data[BSIZE];
};

