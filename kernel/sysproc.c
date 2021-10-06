#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64 get_free_mem(void);
uint64 get_used_proc(void);

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

// 给当前的进程增长n个bytes的内存.如果成功,返回增长之前的占有量;如果失败,返回-1
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

// trace the asked process
uint64
sys_trace(void)
{
  int trace_mask;
  struct proc *p = myproc();
  if(argint(0,&trace_mask)<0){
    return -1;
  }
  p->trace_mask = trace_mask;
  return 0;
}

uint64
sys_sysinfo(void)
{
  struct proc *p = myproc();
  struct sysinfo p_kernel;
  uint64 p_user;
  p_kernel.freemem = get_free_mem();
  p_kernel.nproc = get_used_proc();
  if(argaddr(0,&p_user)<0){
    return -1;
  }

  if(copyout(p->pagetable, p_user, (char *)&p_kernel, sizeof(struct sysinfo)) < 0)
      return -1;
  return 0;
}