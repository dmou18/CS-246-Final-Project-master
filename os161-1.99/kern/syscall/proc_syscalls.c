#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"

#if OPT_A2
#include <mips/trapframe.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>
#endif

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

#if OPT_A2
  bool destroyed = false;
  
 // lock_acquire(curproc->c_lock);
  curproc->zombie = true;
  curproc->exit_code = _MKWAIT_EXIT(exitcode);
  if (curproc->died != NULL) {
    V(curproc->died);
  }
//  lock_release(curproc->c_lock);

  struct proc *parent = curproc->parent;
  if (parent != NULL) {
 //   lock_acquire(parent->c_lock);
    if (parent->zombie == true) {
 //    lock_release(parent->c_lock);
     destroyed = true;
    } else {
 //   lock_release(parent->c_lock);
    }
  } else {
    destroyed = true;
  }

 
  int mychildren_count = curproc->children->num;
  bool allzombie = true;
  for(int j = 0; j < mychildren_count; ++j) {
    struct proc *mychild = (struct proc *)array_get(curproc->children, j);
    //lock_acquire(mychild->c_lock);
    if (mychild != NULL) {
        mychild->parent = NULL;
        if (mychild->zombie == false) {
            allzombie = false;
        }
    }
  }
  if (allzombie) {
    for(int k = 0; k < mychildren_count; ++k) {
        struct proc *child = (struct proc *)array_get(curproc->children, k);
      //  lock_release(child->c_lock);
        proc_destroy(child);
    }
/*  } else {
    for (int j = mychildren_count -1; j >= 0; --j) {
        struct proc *mychild = (struct proc *)array_get(curproc->children, j);
        lock_release(mychild->c_lock);
    } */
  }


#else 
  (void)exitcode;

#endif

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

#if OPT_A2
    if (destroyed) {
        proc_destroy(p);
    }
#else
  proc_destroy(p);

#endif
 
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  if (curproc->pid <= 0) {
    panic("sys_getpid get pid less than or equal to 0\n");
  }
  *retval = curproc->pid;
  return(0);
#else
  *retval = 0;
  return(0);
#endif

}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  if (options != 0) {
    return(EINVAL);
  }
#if OPT_A2
  int count = curproc->children->num;
 
  for(int i = 0; i < count; i++) {
    struct proc *child = (struct proc*)array_get(curproc->children, i);
    if(child->pid == pid) {
    lock_acquire(child->c_lock);
        if (child->zombie == false) {
            child->died = sem_create("sem", 0);
            lock_release(child->c_lock);
            P(child->died);
        } else {
            lock_release(child->c_lock);
        }
        exitstatus = child->exit_code;
        proc_destroy(child);
        break;
    }
  }

  result = copyout((void *)&exitstatus, status, sizeof(int));
  if(result) {
    return result;
  }
  *retval = pid;
  return 0;

#else
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}

#if OPT_A2
int sys_fork(struct trapframe *tf, pid_t *retval) {
    struct proc *newproc = proc_create_runprogram("child");
    if (newproc == NULL) {
        panic("Failed to call proc_create_runprogram \n");
    }

    newproc->parent = curproc;
    
   // struct proc_info child_info = {newproc, newproc->pid, -1};

    lock_acquire(curproc->c_lock);
    array_add(curproc->children, newproc,NULL);
    curproc->children_count += 1;
    lock_release(curproc->c_lock);
    
    int check = as_copy(curproc_getas(), &(newproc->p_addrspace));
    if (check != 0) {
        panic("as_copy() failed \n");
    }
 
    struct trapframe *temp = kmalloc(sizeof(struct trapframe));
    *temp = *tf;
    //curproc->tf = tf;
    check = thread_fork(newproc->p_name, newproc, &enter_forked_process, (void*)temp, 0);
    if (check) {
        kfree(temp);
        panic("thread_fork failed \n"); 

    }

    *retval = newproc->pid;

    return 0;
}



int sys_execv(const char *program, char **args) {	
    struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

    // count the number of arguments and copy them to kernel
    int args_num = 0;
    for (int i = 0; args[i] != NULL; i++) {
        ++args_num;
    }

    size_t args_size =(args_num + 1) * sizeof(char *);
    char **args_kern = kmalloc(args_size);
    if (args_kern == NULL) {
        panic("Fail to allocate space for args_kern\n");
    }

    args_kern[args_num] = NULL;
    for (int i = 0; i < args_num; ++i) {
        size_t arg_size = sizeof(char) * strlen(args[i] + 1);
        char *arg_kern = kmalloc(arg_size);
        if (arg_kern == NULL) {
            panic("Fail to allocate space for string argument\n");
        }
        result = copyinstr((const_userptr_t) args[i], arg_kern, strlen(args[i]) + 1, NULL);
        if (result) {
            panic("Fail to copy string argument into the kernel space\n");
        }
        args_kern[i] = arg_kern;
    }
    
    // Copy the program path to kernel
    int program_len = strlen(program) + 1;
    size_t program_size = program_len *sizeof(char);
    char *program_kern = kmalloc(program_size);
    if (program_kern == NULL) {
        panic("Fail to allocate space for grogram name \n");
    }
    
    result = copyinstr((const_userptr_t) program, (void *) program_kern, program_size, NULL);
    if (result) {
        panic("Fail to copy program name into kernel \n");
    }
    
	/* Open the file. */
	result = vfs_open((char *)program_kern, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	struct addrspace *oldas = curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr, args_kern,args_num);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}
	
    // Destroty old as and free heap allocated objects
    kfree(program_kern);
    for (int i = 0; i < args_num; ++i) {
        kfree(args_kern[i]);
    }
    kfree(args_kern);
    as_destroy(oldas);

	/* Warp to user mode. */
	enter_new_process(args_num /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
            stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

#endif 
