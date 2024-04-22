#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define SNU 1

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
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

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
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

#ifdef SNU
/* Do not touch sys_time() */
uint64 
sys_time(void)
{
  uint64 x;

  asm volatile("rdtime %0" : "=r" (x));
  return x;
}
/* Do not touch sys_time() */

uint64
sys_sched_setattr(void)
{
  // FILL HERE
  uint64 pid, runtime, period;
  //sys_sched_setattr(int pid, int runtime, int period);
  //read the arguments from trapframe
  argaddr(0, &pid);
  argaddr(1, &runtime);
  argaddr(2, &period);
  //print the arguments
  //printf("pid: %d, runtime: %d, period: %d\n", pid, runtime, period);
  //set the attributes to the process
  int err = sched_setattr(pid, runtime, period);
  return err;
}

uint64
sys_sched_yield(void)
{
  // MODIFY THIS
  //call the sched_yield function
  //check whether the current process is normal process or real-time process
  //if the current process is real-time process, then call the sched_yield function
  //if the current process is normal process, then call the yield function
  //return 0 if the function is successfully executed
  //return -1 if the function is failed
  struct proc *p = myproc();
  if(p->period == 0){
    yield();
  }
  else{
    sched_yield();
  }
  return 0;
}
#endif
