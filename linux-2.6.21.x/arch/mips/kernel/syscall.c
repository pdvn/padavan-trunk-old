/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997, 2000, 2001, 05 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2001 MIPS Technologies, Inc.
 */
#include <linux/a.out.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/mman.h>
#include <linux/ptrace.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/unistd.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/compiler.h>

#include <asm/branch.h>
#include <asm/cachectl.h>
#include <asm/cacheflush.h>
#include <asm/ipc.h>
#include <asm/asm-offsets.h>
#include <asm/signal.h>
#include <asm/sim.h>
#include <asm/shmparam.h>
#include <asm/sysmips.h>
#include <asm/uaccess.h>

/*
 * For historic reasons the pipe(2) syscall on MIPS has an unusual calling
 * convention.  It returns results in registers $v0 / $v1 which means there
 * is no need for it to do verify the validity of a userspace pointer
 * argument.  Historically that used to be expensive in Linux.  These days
 * the performance advantage is negligible.
 */
asmlinkage int sys_pipe(nabi_no_regargs volatile struct pt_regs regs)
{
	int fd[2];
	int error, res;

	error = do_pipe(fd);
	if (error) {
		res = error;
		goto out;
	}
	regs.regs[3] = fd[1];
	res = fd[0];
out:
	return res;
}

/* common code for old and new mmaps */
static inline unsigned long
do_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
        unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	unsigned long error = -EBADF;
	struct file * file = NULL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:
	return error;
}

asmlinkage unsigned long
old_mmap(unsigned long addr, unsigned long len, int prot,
	int flags, int fd, off_t offset)
{
	unsigned long result;

	result = -EINVAL;
	if (offset & ~PAGE_MASK)
		goto out;

	result = do_mmap2(addr, len, prot, flags, fd, offset >> PAGE_SHIFT);

out:
	return result;
}

asmlinkage unsigned long
sys_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
          unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	if (pgoff & (~PAGE_MASK >> 12))
		return -EINVAL;

	return do_mmap2(addr, len, prot, flags, fd, pgoff >> (PAGE_SHIFT-12));
}

save_static_function(sys_fork);
__attribute_used__ noinline static int
_sys_fork(nabi_no_regargs struct pt_regs regs)
{
	return do_fork(SIGCHLD, regs.regs[29], &regs, 0, NULL, NULL);
}

save_static_function(sys_clone);
__attribute_used__ noinline static int
_sys_clone(nabi_no_regargs struct pt_regs regs)
{
	unsigned long clone_flags;
	unsigned long newsp;
	int __user *parent_tidptr, *child_tidptr;

	clone_flags = regs.regs[4];
	newsp = regs.regs[5];
	if (!newsp)
		newsp = regs.regs[29];
	parent_tidptr = (int __user *) regs.regs[6];
#ifdef CONFIG_32BIT
	/* We need to fetch the fifth argument off the stack.  */
	child_tidptr = NULL;
	if (clone_flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) {
		int __user *__user *usp = (int __user *__user *) regs.regs[29];
		if (regs.regs[2] == __NR_syscall) {
			if (get_user (child_tidptr, &usp[5]))
				return -EFAULT;
		}
		else if (get_user (child_tidptr, &usp[4]))
			return -EFAULT;
	}
#else
	child_tidptr = (int __user *) regs.regs[8];
#endif
	return do_fork(clone_flags, newsp, &regs, 0,
	               parent_tidptr, child_tidptr);
}

/*
 * sys_execve() executes a new program.
 */
asmlinkage int sys_execve(nabi_no_regargs struct pt_regs regs)
{
	int error;
	char * filename;

	filename = getname((char __user *) (long)regs.regs[4]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, (char __user *__user *) (long)regs.regs[5],
	                  (char __user *__user *) (long)regs.regs[6], &regs);
	putname(filename);

out:
	return error;
}

/*
 * Compacrapability ...
 */
asmlinkage int sys_uname(struct old_utsname __user * name)
{
	if (name && !copy_to_user(name, utsname(), sizeof (*name)))
		return 0;
	return -EFAULT;
}

/*
 * Compacrapability ...
 */
asmlinkage int sys_olduname(struct oldold_utsname __user * name)
{
	int error;

	if (!name)
		return -EFAULT;
	if (!access_ok(VERIFY_WRITE, name, sizeof(struct oldold_utsname)))
		return -EFAULT;

	error = __copy_to_user(&name->sysname, &utsname()->sysname,
			       __OLD_UTS_LEN);
	error -= __put_user(0, name->sysname + __OLD_UTS_LEN);
	error -= __copy_to_user(&name->nodename, &utsname()->nodename,
				__OLD_UTS_LEN);
	error -= __put_user(0, name->nodename + __OLD_UTS_LEN);
	error -= __copy_to_user(&name->release, &utsname()->release,
				__OLD_UTS_LEN);
	error -= __put_user(0, name->release + __OLD_UTS_LEN);
	error -= __copy_to_user(&name->version, &utsname()->version,
				__OLD_UTS_LEN);
	error -= __put_user(0, name->version + __OLD_UTS_LEN);
	error -= __copy_to_user(&name->machine, &utsname()->machine,
				__OLD_UTS_LEN);
	error = __put_user(0, name->machine + __OLD_UTS_LEN);
	error = error ? -EFAULT : 0;

	return error;
}

asmlinkage int sys_set_thread_area(unsigned long addr)
{
	struct thread_info *ti = task_thread_info(current);

	ti->tp_value = addr;
	if (cpu_has_userlocal)
		write_c0_userlocal(addr);

	return 0;
}

asmlinkage int _sys_sysmips(int cmd, long arg1, int arg2, int arg3)
{
	int	tmp;

	switch(cmd) {
	case MIPS_ATOMIC_SET:
		printk(KERN_CRIT "How did I get here?\n");
		return -EINVAL;

	case MIPS_FIXADE:
		tmp = current->thread.mflags & ~3;
		current->thread.mflags = tmp | (arg1 & 3);
		return 0;

	case FLUSH_CACHE:
		__flush_cache_all();
		return 0;
	}

	return -EINVAL;
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */
asmlinkage int sys_ipc(unsigned int call, int first, int second,
		       unsigned long third, void __user *ptr, long fifth)
{
	int version, ret;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	switch (call) {
	case SEMOP:
		return sys_semtimedop(first, (struct sembuf __user *)ptr,
		                      second, NULL);
	case SEMTIMEDOP:
		return sys_semtimedop(first, (struct sembuf __user *)ptr,
				      second,
				      (const struct timespec __user *)fifth);
	case SEMGET:
		return sys_semget(first, second, third);
	case SEMCTL: {
		union semun fourth;
		if (!ptr)
			return -EINVAL;
		if (get_user(fourth.__pad, (void __user *__user *) ptr))
			return -EFAULT;
		return sys_semctl(first, second, third, fourth);
	}

	case MSGSND:
		return sys_msgsnd(first, (struct msgbuf __user *) ptr,
				  second, third);
	case MSGRCV:
		switch (version) {
		case 0: {
			struct ipc_kludge tmp;
			if (!ptr)
				return -EINVAL;

			if (copy_from_user(&tmp,
					   (struct ipc_kludge __user *) ptr,
					   sizeof(tmp)))
				return -EFAULT;
			return sys_msgrcv(first, tmp.msgp, second,
					  tmp.msgtyp, third);
		}
		default:
			return sys_msgrcv(first,
					  (struct msgbuf __user *) ptr,
					  second, fifth, third);
		}
	case MSGGET:
		return sys_msgget((key_t) first, second);
	case MSGCTL:
		return sys_msgctl(first, second,
				  (struct msqid_ds __user *) ptr);

	case SHMAT:
		switch (version) {
		default: {
			unsigned long raddr;
			ret = do_shmat(first, (char __user *) ptr, second,
				       &raddr);
			if (ret)
				return ret;
			return put_user(raddr, (unsigned long __user *) third);
		}
		case 1:	/* iBCS2 emulator entry point */
			if (!segment_eq(get_fs(), get_ds()))
				return -EINVAL;
			return do_shmat(first, (char __user *) ptr, second,
				        (unsigned long *) third);
		}
	case SHMDT:
		return sys_shmdt((char __user *)ptr);
	case SHMGET:
		return sys_shmget(first, second, third);
	case SHMCTL:
		return sys_shmctl(first, second,
				  (struct shmid_ds __user *) ptr);
	default:
		return -ENOSYS;
	}
}

/*
 * No implemented yet ...
 */
asmlinkage int sys_cachectl(char *addr, int nbytes, int op)
{
	return -ENOSYS;
}

/*
 * If we ever come here the user sp is bad.  Zap the process right away.
 * Due to the bad stack signaling wouldn't work.
 */
asmlinkage void bad_stack(void)
{
	do_exit(SIGSEGV);
}

/*
 * Do a system call from kernel instead of calling sys_execve so we
 * end up with proper pt_regs.
 */
int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
	register unsigned long __a0 asm("$4") = (unsigned long) filename;
	register unsigned long __a1 asm("$5") = (unsigned long) argv;
	register unsigned long __a2 asm("$6") = (unsigned long) envp;
	register unsigned long __a3 asm("$7");
	unsigned long __v0;

	__asm__ volatile ("					\n"
	"	.set	noreorder				\n"
	"	li	$2, %5		# __NR_execve		\n"
	"	syscall						\n"
	"	move	%0, $2					\n"
	"	.set	reorder					\n"
	: "=&r" (__v0), "=r" (__a3)
	: "r" (__a0), "r" (__a1), "r" (__a2), "i" (__NR_execve)
	: "$2", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24",
	  "memory");

	if (__a3 == 0)
		return __v0;

	return -__v0;
}
