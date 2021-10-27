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
} kmem[NCPU];

void 
kmem_init()
{
  char name[10] ="kmem";
  
  for (int i=1;i<NCPU;i++){
    // name[5]=i+48;
    // name[6]=0;
    initlock(&kmem[i].lock,name);
    if(i==1){
      freerange((void*)(PGROUNDDOWN((PHYSTOP-(uint64)end)/3)+end),(void*)(PGROUNDDOWN(2*(PHYSTOP-(uint64)end)/3)+end));
    }
    else if(i==2)
    {
      freerange((void*)(PGROUNDDOWN(2*(PHYSTOP-(uint64)end)/3)+end),(void*)PHYSTOP);
    }
  }
}

void
kinit()
{
  initlock(&kmem[0].lock, "kmem");
  freerange(end, (void*)(PGROUNDDOWN((PHYSTOP-(uint64)end)/3)+end));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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
  int cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
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
  int cpu_id = cpuid();
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  else{
    r=steal_others(cpu_id);
    if(r)
      kmem[cpu_id].freelist = r->next;
  }
  release(&kmem[cpu_id].lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

struct run*
steal_others(int cpu_id)
{
  struct run* r=0;
  for(int i=0;i<NCPU;i++){
    if(i==cpu_id){
      continue;
    }
    acquire(&kmem[i].lock);
    r=kmem[i].freelist;
    if(r){
      kmem[i].freelist=r->next;
      r->next=0;
      kmem[cpu_id].freelist=r; //分配第一页
      r=kmem[i].freelist;//r重新指向i的头
      for(int j=0;j<9&&r!=0;j++){ // 再分配四页

        kmem[i].freelist=r->next;
        r->next = kmem[cpu_id].freelist;
        kmem[cpu_id].freelist=r;
        r=kmem[i].freelist;
        
      }
      release(&kmem[i].lock);
      break;
    }
    else{
      release(&kmem[i].lock);
      continue;
    }

  }
  
  return kmem[cpu_id].freelist;
}

