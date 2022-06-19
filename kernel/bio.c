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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf* pbuf[NBUF];
  struct buf* pindex[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  uint maxScore;
  uint minScore;
} bcache;

void
binit(void)
{
  //struct buf *b;
  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.maxScore = 0;//初始化maxScore
  bcache.minScore = 0;//初始化maxScore
  for (int i = 0; i < NBUF; i++) {
    bcache.buf[i].score = 0;//初始化分数表
    bcache.buf[i].index = i;
    bcache.pbuf[i] = &bcache.buf[i];
    bcache.pindex[i] = &bcache.buf[i];
    initsleeplock(&bcache.pbuf[i]->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  acquire(&bcache.lock);

  // Is the block already cached?**bget** (kernel/bio.c:59)扫描buffer链表，寻找给定设备号和扇区号来查找缓冲区(kernel/bio.c:65-73)。如果存在，**bget**就会获取该buffer的sleep-lock。然后**bget**返回被锁定的buffer。
  //printf("%d %d ////\n", dev, blockno);
  for (ushort i = 0; i < NBUF; i++) {//从前往后找需要的buffer
    //printf("%d ++++++++++++\n", b->blockno);
    //printf("%d %d %d////\n", dev, blockno,i);
    if (bcache.pbuf[i]->dev == dev && bcache.pbuf[i]->blockno == blockno) {
      bcache.pbuf[i]->refcnt++;
      release(&bcache.lock);
      acquiresleep(&bcache.pbuf[i]->lock);
      return bcache.pbuf[i];
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (ushort i = 0; i < NBUF; i++) {//从后往前找被brelse次数最少的buffer，如果b->refcnt == 0则重置该buffer
    if (bcache.pbuf[i]->refcnt == 0) {
      bcache.pbuf[i]->dev = dev;
      bcache.pbuf[i]->blockno = blockno;
      bcache.pbuf[i]->valid = 0;
      bcache.pbuf[i]->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&bcache.pbuf[i]->lock);
      //printf("Not cached.Not cached.Not cached.Not cached.Not cached.Not cached.Not cached.Not cached.Not cached.%d\n", i);
      return bcache.pbuf[i];
    }
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

void
bswap(ushort a, ushort b) {
  bcache.pbuf[a]->index = b;
  bcache.pbuf[b]->index = a;
  struct buf* temp = bcache.pbuf[a];
  bcache.pbuf[a] = bcache.pbuf[b];
  bcache.pbuf[b] = temp;
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");
  //printf("brelse %d\n", b->index);
  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  b->score++;
  ushort index = b->index;
  /*  if (b->refcnt == 0) {
      // no one is waiting for it.
      b->next->prev = b->prev;
      b->prev->next = b->next;
      b->next = bcache.head.next;
      b->prev = &bcache.head;//b换到链表头下一个的位置
      bcache.head.next->prev = b;
      bcache.head.next = b;
    }*/
  while (1) {
    if (2 * index + 1 >= NBUF)break;
    if (index == 14) {
      //printf("time for 14 *****************************************************************************************************************************************************************************************************************************************************************\n");
      if (bcache.pbuf[29]->score >= bcache.pbuf[14]->score)break;
      //printf("time for 14 //////////////////////////////////////////////////\n");
      bswap(14, 29);
      break;
    }
    else {
      if (bcache.pbuf[2 * index + 2]->score >= bcache.pbuf[index]->score && bcache.pbuf[2 * index + 1]->score >= bcache.pbuf[index]->score)break;
      ushort target = (bcache.pbuf[2 * index + 2]->score < bcache.pbuf[2 * index + 1]->score) ? (2 * index + 2) : (2 * index + 1);
      bswap(index, target);
      index = target;
    }
  }

  /*  while (index > 0 && bcache.pbuf[(index - 1) / 2]->score > bcache.pbuf[index]->score) {//更新堆
      printf("update %d %d\n", index, bcache.pbuf[index]->score);
      bcache.pbuf[(index - 1) / 2]->index = index;
      bcache.pbuf[index]->index = (index - 1) / 2;
      struct buf* temp = bcache.pbuf[(index - 1) / 2];
      bcache.pbuf[(index - 1) / 2] = bcache.pbuf[index];
      bcache.pbuf[index] = temp;
      index = (index - 1) / 2;
    }*/
    //for (int k = 0; k < NBUF; k++) {
      //printf("%d-", bcache.pbuf[k]->score);
    //}
    //printf("\n");
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


