#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
//#include "vm.h"
#include "fcntl.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
	struct proc *currproc = myproc();

	if(addr >= currproc->sz || addr+4 > currproc->sz)
		return -1;
	*ip = *(int*)(addr);
	return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
	char *s, *ep;
	struct proc *currproc = myproc();

	if(addr >= currproc->sz)
		return -1;
	*pp = (char*)addr;
	ep = (char*)currproc->sz;
	for(s = *pp; s < ep; s++){
		if(*s == 0)
			return s - *pp;
	}
	return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
	return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
	int i;
	struct proc *currproc = myproc();

	if(argint(n, &i) < 0)
		return -1;
	if(size < 0 || (uint)i >= currproc->sz || (uint)i+size > currproc->sz)
		return -1;
	*pp = (char*)i;
	return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
	int addr;
	if(argint(n, &addr) < 0)
		return -1;
	return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_shm_open(void);
extern int sys_shm_trunc(void);
extern int sys_shm_map(void);
extern int sys_shm_close(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_shm_open] sys_shm_open,
[SYS_shm_trunc] sys_shm_trunc,
[SYS_shm_map] sys_shm_map,
[SYS_shm_close] sys_shm_close,
};

#define MAX_SHM_OBJECTS 64
struct shm_object shm_objects[MAX_SHM_OBJECTS];

int sys_shm_open(void)
{
  char *name;
  struct proc *currproc = myproc();
  if(argstr(0, &name) < 0)
    return -1;

  if(strlen(name) == 0) 
    return -1;

  int s = 0;
  for (int i =0 ; i<16;i++){
    if(currproc->shm_max[i]!=0){
      s++;
    }
  }
  for(int i = 0; i < MAX_SHM_OBJECTS; i++) {
    if(shm_objects[i].name[0] != 0 && strncmp(shm_objects[i].name, name, strlen(shm_objects[i].name)) == 0) {
      for(int i = 0 ; i<16;i++){
        if(currproc->shm_max[i]==0){
          currproc->shm_max[i] = &shm_objects[i];
          shm_objects[i].ref_count++;
          break;
        } 
      }
      return i; 
    }
  }
  for(int i = 0; i < MAX_SHM_OBJECTS; i++) {
    if(shm_objects[i].name[0] == 0) {
      for(int i = 0 ; i<16;i++){
        if(currproc->shm_max[i]==0){
          currproc->shm_max[i] = &shm_objects[i];
          strncpy(shm_objects[i].name, name, 256);
          shm_objects[i].size = 0;
          shm_objects[i].ref_count = 1;
          break;
        }
      }
      return i; 
    }
  }
  return -1;
}

int sys_shm_trunc(void)
{
 
  int fd, size;
  if(argint(0, &fd) < 0 || argint(1, &size) < 0)
    return -1;
  
  if(fd < 0 || shm_objects[fd].name[0] == 0 || fd >= MAX_SHM_OBJECTS)
    return -1;

  if(size <= 0)
    return -1;

  if(shm_objects[fd].size != 0)
    return -1;

  //int num_pages = PGROUNDUP(size);
  int num_pages = (size + PGSIZE - 1) / PGSIZE;
  
  for(int i=0; i<num_pages; i++)
  {
    shm_objects[fd].pages[i] = kalloc();
    if(shm_objects[fd].pages[i] == 0)
    {
      for(int j = 0; j < i; j++){
      kfree(shm_objects[fd].pages[j]);
    }
    return -1;
    }
    memset(shm_objects[fd].pages[i], 0, PGSIZE);
  }
  shm_objects[fd].size = PGSIZE*num_pages;
  return shm_objects[fd].size;
    
}

int sys_shm_map(void)
{
  int fd, addr;
  struct proc *currproc = myproc();

  if(argint(0, &fd) < 0 || argint(1, &addr) < 0)
    return -1;

  int num_pages = (shm_objects[fd].size + PGSIZE - 1) / PGSIZE;
 
  if(shm_objects[fd].name[0] == 0 || fd >= MAX_SHM_OBJECTS || fd < 0)
    return -1;

  for(int i = 0; i < num_pages; i++) {
    pte_t *pte = walkpgdir(currproc->pgdir, (void *)(addr + i * PGSIZE), 0);
    if((*pte & PTE_P)&&pte)
      return -1;
  }

  if(mappages(currproc->pgdir, (void *)addr, num_pages * PGSIZE, V2P(shm_objects[fd].pages[0]), PTE_W | PTE_U) < 0)
    return -1;

  return 1;
}

int sys_shm_close(void)
{
    int fd;
    struct proc *currproc = myproc();
    if(argint(0, &fd) < 0)
        return -1;

    if(fd < 0 || fd >= MAX_SHM_OBJECTS || shm_objects[fd].name[0] == 0)
        return -1;

    shm_objects[fd].ref_count--;
    currproc->shm_max[fd] = 0;

    if(shm_objects[fd].ref_count == 0) {
        for (int i = 0; i < 32; i++) {
            if (shm_objects[fd].pages[i] != 0) {
                kfree(shm_objects[fd].pages[i]);
                shm_objects[fd].pages[i] = 0;
            }
        }
        shm_objects[fd].name[0] = 0;
    }
    return 0;
    
}

void
syscall(void)
{
	int num;
	struct proc *currproc = myproc();

	num = currproc->tf->eax;
	if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
		currproc->tf->eax = syscalls[num]();
	} else {
		cprintf("%d %s: unknown sys call %d\n",
			currproc->pid, currproc->name, num);
		currproc->tf->eax = -1;
	}
}
