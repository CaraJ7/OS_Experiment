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

#define NBUCKETS 13

struct {
  struct spinlock recycle_lock;
  int is_locked[NBUCKETS];
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf buckets[NBUCKETS]; // buckets是双向链表空的表头
} bcache;


void
binit(void)
{
  struct buf *b;
  initlock(&bcache.recycle_lock, "bcache_recycle");
  // initlock(&bcache.lock, "bcache");
  b=bcache.buf;
  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i],"bcache");
    bcache.buckets[i].prev = & bcache.buckets[i];
    bcache.buckets[i].next = & bcache.buckets[i];
    if (i<(NBUCKETS-1)){
      for(int j=0;j<(int)(NBUF/NBUCKETS);j++){
        b->next = bcache.buckets[i].next;
        b->prev = &bcache.buckets[i];
        initsleeplock(&b->lock, "buffer"); //防止一个buffer被多访问
        bcache.buckets[i].next->prev = b;
        bcache.buckets[i].next = b;
        b++;
      }
    }
    else{ // 最后一个，要分配剩下的所有
      for(int j=0;j<(NBUF-i*(int)(NBUF/NBUCKETS));j++){
        b->next = bcache.buckets[i].next;
        b->prev = &bcache.buckets[i];
        initsleeplock(&b->lock, "buffer"); //防止一个buffer被多访问
        bcache.buckets[i].next->prev = b;
        bcache.buckets[i].next = b;
        b++;
      }
    }
  }
}


// void
// binit(void)
// {
//   struct buf *b;

//   initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head; // 初始化双向链表，前后指针均指向自己
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head; //头插法
  //   initsleeplock(&b->lock, "buffer"); //防止一个buffer被多访问
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
// }

int 
next(int i,int* isvisited){
  while(1){
    i=i+1;
    i = i%NBUCKETS;
    if(isvisited[i]==0){
      printf("next i is %d\n",i);
      return i;
    }
  }
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock[blockno%NBUCKETS]);
  

  // Is the block already cached?
  for(b = bcache.buckets[blockno%NBUCKETS].next; b != &bcache.buckets[blockno%NBUCKETS]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[blockno%NBUCKETS]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  // 先找桶内的
  for(b = bcache.buckets[blockno%NBUCKETS].prev; b != &bcache.buckets[blockno%NBUCKETS]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[blockno%NBUCKETS]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  
  // // 桶内没有，要从桶外找
  for(int i=0;i<NBUCKETS;i++){
    if(i==blockno%13)
      continue;

       // printf("already in lock %d ask for lock %d\n",blockno%13,i);

    // ...原来居然是差在这一句，这里把这个锁释放了也没事。
    //直接考虑死锁情况，那一定是拿到了blockno%NBUCKETS锁的同时要去拿另外一个，但是这个句子就直接保证他根本不能拿到blockno%NBUCKETS的同时再去拿别人
    // 同时这种情况也不会出错，因为这个锁下面的操作都是在做插入，不会有什么其他的更改。即使释放掉以后，在其他的地方又对这个桶进行了更改，也并不影响这个进程接下来要做的事情
    // 当然，这种情况也是可能出错的，但是比较极限一点。就是哈希疯狂冲突，这个桶里面的元素非常多，别的桶的元素也都用完了。但是这个时候同时，这个桶又释放了很多的元素，那么这个时候他是检测不到的
    // emmm我想了想，好像别的实现方法也考虑不到这种情况，因为不会再回去检查自己的桶了。
    release(&bcache.lock[blockno%NBUCKETS]);
 
    acquire(&bcache.lock[i]); 
    acquire(&bcache.lock[blockno%NBUCKETS]);
    for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
      if(b->refcnt == 0){
        // printf("find vacancy in %d\n",i);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 从原来的里面取出来
        b->prev->next = b->next;
        b->next->prev = b->prev;
        // 插入到新的头上
        b->next = bcache.buckets[blockno%NBUCKETS].next ;
        b->prev = &bcache.buckets[blockno%NBUCKETS];
        bcache.buckets[blockno%NBUCKETS].next = b;
        b->next->prev = b;
        // 释放锁
        release(&bcache.lock[i]);
        release(&bcache.lock[blockno%NBUCKETS]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }
    
// 以下为尝试，会产生不知道哪里的死锁
  // 告诉别人，你不要来我这里搜
  // acquire(&bcache.recycle_lock);
  // bcache.is_locked[blockno%NBUCKETS]=1;
  // release(&bcache.recycle_lock);
  // int cnt=NBUCKETS-1;
  // int i=0;
  // int otherlock=0;
  // int isvisited[NBUCKETS]={0};
  // isvisited[blockno%NBUCKETS]=1;

  // // 桶内没有，要从桶外找
  // while(cnt!=0){
  //   if(i==blockno%NBUCKETS)
  //     continue;
  //   // 看别人现在有没有进入破产状态，即使稍后更改也不会有事，因为对方不会来要我的锁
  //   acquire(&bcache.recycle_lock);
  //   otherlock = bcache.is_locked[i%NBUCKETS];
  //   release(&bcache.recycle_lock);
  //   printf("looking for other lock is %d other lock is %d\n",i%NBUCKETS,otherlock);

  //   if(otherlock==1){
  //     release(&bcache.recycle_lock);
  //     i=next(i,isvisited);
  //     continue;
  //   }

  //   isvisited[i]=1;
  //   cnt--;

  //   printf("already in lock %d ask for lock %d\n",blockno%13,i);
  //   acquire(&bcache.lock[i]);
  //   for(b = bcache.buckets[i].next; b != &bcache.buckets[i]; b = b->next){
  //     if(b->refcnt == 0){
  //       printf("find vacancy in %d\n",i);
  //       b->dev = dev;
  //       b->blockno = blockno;
  //       b->valid = 0;
  //       b->refcnt = 1; 
  //       // 从原来的里面取出来
  //       b->prev->next = b->next;
  //       b->next->prev = b->prev;
  //       // 插入到新的头上
  //       b->next = bcache.buckets[blockno%NBUCKETS].next ;
  //       b->prev = &bcache.buckets[blockno%NBUCKETS];
  //       bcache.buckets[blockno%NBUCKETS].next = b;
  //       b->next->prev = b;
  //       // 释放锁
  //       release(&bcache.lock[i]);
  //       release(&bcache.lock[blockno%NBUCKETS]);
  //       acquiresleep(&b->lock);

  //       acquire(&bcache.recycle_lock);
  //       bcache.is_locked[blockno%NBUCKETS]=0;
  //       release(&bcache.recycle_lock);
      

  //       return b;
  //     }
  //   }
  //   release(&bcache.lock[i]);
  //   i=next(i,isvisited);

  // }
  
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
// void
// brelse(struct buf *b)
// {
//   if(!holdingsleep(&b->lock))
//     panic("brelse");

//   releasesleep(&b->lock); //这里似乎没影响。顺序重要，加锁的顺序必须固定，先bcache.lock然后buffer.lock，如果已经有了buffer.lock，就必须先释放再申请bcache.lock

//   acquire(&bcache.lock); //操作了链表。记录谁被使用，谁没有被使用。
//   // releasesleep(&b->lock); 应该放在这里也是可以的，不会死锁
//   b->refcnt--;
//   if (b->refcnt == 0) {
//     // no one is waiting for it.
//     b->next->prev = b->prev;
//     b->prev->next = b->next;
//     b->next = bcache.head.next;
//     b->prev = &bcache.head;
//     bcache.head.next->prev = b;
//     bcache.head.next = b;
//   }
  
//   release(&bcache.lock);
// }

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock); //这里似乎没影响。顺序重要，加锁的顺序必须固定，先bcache.lock然后buffer.lock，如果已经有了buffer.lock，就必须先释放再申请bcache.lock

  acquire(&bcache.lock[b->blockno%NBUCKETS]); //操作了链表。记录谁被使用，谁没有被使用。
  // releasesleep(&b->lock); 应该放在这里也是可以的，不会死锁
  b->refcnt--;
  if (b->refcnt == 0) { // 插到了队头
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[b->blockno%NBUCKETS].next;
    b->prev = &bcache.buckets[b->blockno%NBUCKETS];
    bcache.buckets[b->blockno%NBUCKETS].next->prev = b;
    bcache.buckets[b->blockno%NBUCKETS].next = b;
  }
  
  release(&bcache.lock[b->blockno%NBUCKETS]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKETS]);
  b->refcnt++;
  release(&bcache.lock[b->blockno%NBUCKETS]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKETS]);
  b->refcnt--;
  release(&bcache.lock[b->blockno%NBUCKETS]);
}


