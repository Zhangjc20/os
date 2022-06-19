struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint64 score; //堆维护指标
  uint64 itime; //堆维护指标
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  ushort index;
};

