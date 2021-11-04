#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
// #include "proc.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

/*
 * 建立一个kernel page table，为每个进程创建
 * ppk指per process kernel
 */
pagetable_t
ppk_kvminit()
{
  pagetable_t ppk_pagetable;
  ppk_pagetable = (pagetable_t) kalloc();
  memset(ppk_pagetable, 0, PGSIZE);

  // uart registers
  ppk_kvmmap(ppk_pagetable,UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  ppk_kvmmap(ppk_pagetable,VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  // ppk_kvmmap(ppk_pagetable,CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  ppk_kvmmap(ppk_pagetable,PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  ppk_kvmmap(ppk_pagetable,KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  ppk_kvmmap(ppk_pagetable,(uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  ppk_kvmmap(ppk_pagetable,TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return ppk_pagetable;
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  // if(va==0x2000000)
  //   printf("pa address is %p\n",);
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0){
    printf("pte=0\n");
    return 0;}
  if((*pte & PTE_V) == 0){
    printf("PTE_V=0\n");
    return 0;}
  if((*pte & PTE_U) == 0){
    printf("PTE_U=0\n");
    return 0;}
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 修改上面的函数，使其改变的是新的页表,多了一个参数
// only used when booting.
// does not flush TLB or enable paging.
void
ppk_kvmmap(pagetable_t pagetable,uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("ppk_kvmmap");
}

// 根据这个进程原来内核栈的虚拟地址和每个进程自己的新的内核的页表，
// 将这个进程的内核栈映射到自己的新的内核页表上
// 改自walkaddr
void
ppk_mapkernelstack(pagetable_t ppk_pagetable,uint64 va){
  pte_t *pte;
  uint64 pa;
  pte = walk(kernel_pagetable, va, 0);
  pa = PTE2PA(*pte);// physical address of kernel stack
  ppk_kvmmap(ppk_pagetable,va,pa, PGSIZE, PTE_R | PTE_W);
  return;
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kernel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V){
      printf("pagetable address is %p\nva is %p\npa is %p\n",pagetable,va,pa);
      printf("already pa is %p\n",PTE2PA(*pte));
      panic("remap");
      // printf("remap");
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
// If do_free != 0, physical memory will be freed
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0 ){
      printf("va is %p\nnpages is %p\na is %p\npagetable is %p\n",va,npages,a,pagetable);
      panic("uvmunmap: walk");
    }
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
// 修改，不能长的太大，不能超过PILC
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz || newsz>(PLIC))
    return oldsz;
  // if(newsz>CLINT)
  //   return -1;
  // printf("old sz is %p new sz is %p\n",oldsz,newsz);
  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; (a < newsz) &&(a<(PLIC)); a += PGSIZE){
    // printf("a is %p\n",a);
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  // if(newsz>PLIC)
  //   return PLIC;
  // else
    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    // printf("uvmunmap from uvmdealloc\n");
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
// gdx,&&最后执行
//if中的前者是来看这个页表项下面还有没有东西
//if的后者则是看他的下层是真正存数据的一段还是又一个页表，是根据后面的这三个标志位来看的。
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;  
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// 改自上面的函数，清空页表，但是不清空其下的子节点
void
ppk_freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      ppk_freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } 
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0){
    // 先从虚拟地址的0开始，free掉除最顶层的tramxxx那两页
    // printf("uvmunmap from uvmfree\n");
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  }
  // 再去free掉页表
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// 就是完全的拷贝，没有什么花样，就是从父进程的0地址开始一页一页的读
// 然后请求一端内存，分给子进程
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// 用来把用户态的页表的内容映射到内核态的页表中
void
user_mapto_kernel(pagetable_t user_pagetable,pagetable_t ppk_pagetable,uint64 sz,uint64 before_sz,int substitution,int perm){
/* 之前可用  
  uint64  pa;
  pte_t *pte;
  
  //sbrk与exec情况，先要把之前在内核上的映射都整没
  if(substitution==1 && before_sz!=0){
    // printf("uvmunmap from user_mapto_kernel\n");
    // printf("before_page size is %d\nnow size is %d\n user pagetable is %p\n",before_sz>>12,sz>>12,user_pagetable);
    uvmunmap(ppk_pagetable,0,before_sz>>12,0);
  }


  // printf("user_map_to_kernel size is %p\n",sz);

  for(int i=0;i<(sz>>12);i++){
  // pa = walkaddr(user_pagetable, i<<12);
    pte = walk(user_pagetable, i<<12, 0);
    if((pte == 0) || ((*pte & PTE_V) == 0))
      panic("user_map_to_kernel pte is wrong");
    pa = PTE2PA(*pte);
    if(pa == 0){ // pa is not mapped
      printf("the problem is in %d\n",i);
      panic("user_mapto_kernel pa not mapped");
    }
    if(mappages(ppk_pagetable, i<<12, PGSIZE, pa, perm) != 0)
        panic("user_mapto_kernel_mappages");
  }
  return;
*/
  uint64  pa;
  pte_t *pte;
  
  // sbrk减内存
  if(substitution==1 && (before_sz>sz)){
    uvmunmap(ppk_pagetable,sz,(before_sz-sz)>>12,0);
    return;
  }
  // exec
  else if(substitution==2){
    uvmunmap(ppk_pagetable,0,before_sz>>12,0);
    for(int i=0;i<(sz>>12);i++){
  // pa = walkaddr(user_pagetable, i<<12);
    pte = walk(user_pagetable, i<<12, 0);
    if((pte == 0) || ((*pte & PTE_V) == 0))
      panic("user_map_to_kernel pte is wrong");
    pa = PTE2PA(*pte);
    if(pa == 0){ // pa is not mapped
      printf("the problem is in %d\n",i);
      panic("user_mapto_kernel pa not mapped");
    }
    if(mappages(ppk_pagetable, i<<12, PGSIZE, pa, perm) != 0)
        panic("user_mapto_kernel_mappages");
    }
    return;
  }
  // else if (substitution==1 && (before_sz<sz))
  //sbrk与exec情况，先要把之前在内核上的映射都整没
  // if(substitution==1 && before_sz!=0){
  //   // printf("uvmunmap from user_mapto_kernel\n");
  //   // printf("before_page size is %d\nnow size is %d\n user pagetable is %p\n",before_sz>>12,sz>>12,user_pagetable);
  //   uvmunmap(ppk_pagetable,0,before_sz>>12,0);
  // }


  // printf("user_map_to_kernel size is %p\n",sz);

  for(int i=(PGROUNDUP(before_sz)>>12);i<(PGROUNDUP(sz)>>12);i++){
  // pa = walkaddr(user_pagetable, i<<12);
    pte = walk(user_pagetable, i<<12, 0);
    if((pte == 0) || ((*pte & PTE_V) == 0))
      panic("user_map_to_kernel pte is wrong");
    pa = PTE2PA(*pte);
    if(pa == 0){ // pa is not mapped
      printf("the problem is in %d\n",i);
      panic("user_mapto_kernel pa not mapped");
    }
    if(mappages(ppk_pagetable, i<<12, PGSIZE, pa, perm) != 0)
        panic("user_mapto_kernel_mappages");
  }
  return;
}

// 检查是否正确的将用户的地址映射到了内核页表上
void
map_check(pagetable_t user_pagetable,pagetable_t ppk_pagetable,uint64 va,uint64 sz,int ifimm){// va没用了
  // uint64 user_pa;
  // uint64  ppk_pa;
  pte_t *user_pte;
  pte_t *ppk_pte;

  // printf("check user_pagetable is %p, size is %p\n",user_pagetable,sz);

  for(int i=0;i<(sz>>12);i++){
  user_pte = walk(user_pagetable, i<<(12), 0);
  if((user_pte == 0) || ((*user_pte & PTE_V) == 0)){
    printf("the wrong i is %d\nsize is %p\n",i,sz);
    printf("ifimm is %d\n",ifimm);
    // panic("map_check 1 is wrong");
    printf("map_check 1 is wrong\n");
  }
  // user_pa = PTE2PA(*user_pte);

  ppk_pte = walk(ppk_pagetable, i<<(12), 0);
  if((ppk_pte == 0) || ((*ppk_pte & PTE_V) == 0)){
    printf("ppk_pagetable is %p\n",ppk_pagetable);
    printf("user_pagetable is %p\n",user_pagetable);
    printf("the wrong i is %d\nsize is %p\n",i,sz);
    printf("ifimm is %d\n",ifimm);
    // panic("map_check 2 is wrong");
    printf("map_check 2 is wrong\n");
    printf("1111111111111111111111111111111111111111111111111111111111111111111111\n");
  }
  // ppk_pa = PTE2PA(*ppk_pte);

  // if(ppk_pa != user_pa){
  //   printf("whole size is %p problem occurs in %p\n",sz,i<<12);
  //   printf("ppk_pa is %p user_pa is %p\n",ppk_pa,user_pa);
  //   // panic("not mapped correct!");
  //   printf("not mapped correct!\n");
  // }
  }
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
// 这里原先的做法是先把不足一页的部分取出来，然后按页一页一页来取
// 因为虚拟空间的一页对应物理空间的一页
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  //test
  // uint64 va0, pa0=0;
  // va0 = PGROUNDDOWN(srcva);
  // pa0 = walkaddr(pagetable, va0);
  // printf("original_right: pa get %d\n",(int*)(pa0 + (srcva - va0)));

  // hw code
  if(copyin_new(pagetable,dst,srcva,len)==0)
  {
    // printf("copyin succeed\n");
    return 0;
  }
  else
  {
    // printf("copyin fail\n");
    return -1; 
  }
  
  // original code
  // uint64 n, va0, pa0=0;
  // // struct proc*  p=  myproc();
  // // pte_t *pte;
  // // uint64  pa;

  // while(len > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);

  //   // printf("original: pa get %p\n",walkaddr(pagetable, srcva));// 从这一步可以看出来，walkaddr返回的是对应的物理地址的页对齐情况，并不带后面的偏移量
  //   // printf("map pa get: %p\n",(pa0 + (srcva - va0)));
    
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > len)
  //     n = len;
  //   memmove(dst, (void *)(pa0 + (srcva - va0)), n);

  //   len -= n;
  //   dst += n;
  //   srcva = va0 + PGSIZE;

 

  // // pte = walk(p->ppk_pagetable, va0, 0);
  // // if((pte == 0) || ((*pte & PTE_V) == 0))
  // //   panic("check proc pte is wrong");
  // // pa = PTE2PA(*pte);
  

  // }

  // // printf("copyin succeed\n");
  // return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  if(copyinstr_new(pagetable, dst,srcva, max)==0){
    return 0;
  }
  else{
    // printf("copyinstr fail\n");
    return -1;
  }
  // uint64 n, va0, pa0;
  // int got_null = 0;

  // while(got_null == 0 && max > 0){
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if(pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if(n > max)
  //     n = max;

  //   char *p = (char *) (pa0 + (srcva - va0));
  //   while(n > 0){
  //     if(*p == '\0'){
  //       *dst = '\0';
  //       got_null = 1;
  //       break;
  //     } else {
  //       *dst = *p;
  //     }
  //     --n;
  //     --max;
  //     p++;
  //     dst++;
  //   }

  //   srcva = va0 + PGSIZE;
  // }
  // if(got_null){
  //   return 0;
  // } else {
  //   return -1;
  // }
}

void
assis_vmprint(pagetable_t pagetable,int level){
  if(level<0)
    return;
  pte_t pte;
  char* a;
  if(level == 2)
    a="||";
  else if(level==1)
    a="|| ||";
  else
    a="|| || ||";
  for(int i=0;i<512;i++){
    pte = pagetable[i];
    if(pte & PTE_V){
      printf("%s%d: pte %p pa %p\n",a,i,pte,PTE2PA(pte));
      assis_vmprint((pagetable_t)PTE2PA(pte),level-1);
    }
  }
  return;
}

void
vmprint(pagetable_t pagetable){
  printf("page table %p\n",pagetable);
  assis_vmprint(pagetable,2);
  return;
}

// check if use global kpgtbl or not 
int 
test_pagetable()
{
  uint64 satp = r_satp();
  uint64 gsatp = MAKE_SATP(kernel_pagetable);
  return satp != gsatp;
}