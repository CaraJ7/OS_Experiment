#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//  第一个和最后一个参数都一定是0，可以不要
uint64 sys_mmap(){
  int addr;
  int length;
  int permission;
  int map_mode;
  int fd;
  struct proc *p = myproc();
  struct file *f;
  if(argint(1, &length) < 0)
    return -1;
  if(argint(2, &permission) < 0)
    return -1;
  if(argint(3, &map_mode) < 0)
    return -1;
  if(argint(4, &fd) < 0)
    return -1;

  // lazy allocation
  // 但是和sbrk不完全相同，p->sz可能不是page-aligned的，增加了以后可能也不是
  // 因此这里采取的一个方法是，把addr向上取，这样可能会空出来一部分(如果addr不是page-aligned的话)
  // 但是应该没有什么影响
  // 后面的那个p->sz应该不用page-aligned，因为即使它不是，后续的也会认为他已经page-aligned了
  addr = PGROUNDUP(p->sz);
  p->sz = addr+length;

  f = p->ofile[fd];
  // ReadonlyFile+MapShared+PROT_WRITE
  // 只读文件 不可shared还是在写,返回错误
  if((!f->writable)&&(map_mode==MAP_SHARED)&&(permission&PROT_WRITE)){
    return -1;
  }

  for(int i=0;i<16;i++){
    if(p->VMA[i].valid==0){
      p->VMA[i].valid = 1;
      p->VMA[i].length = length;
      p->VMA[i].permission = permission;
      p->VMA[i].fd = fd;
      p->VMA[i].start_addr = addr;
      p->VMA[i].actually_mapped_cnt=0;
      p->VMA[i].map_mode = map_mode;
      break;
    }
  }
  
  return addr;
}

uint64 sys_munmap(){
  uint64 addr;
  int length;
  
  if(argaddr(0, &addr) < 0)
    return -1;
  if(argint(1, &length) < 0)
    return -1;
  
  return munmap(addr,length);

}