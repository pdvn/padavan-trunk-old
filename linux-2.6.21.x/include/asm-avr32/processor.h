/*
 * Copyright (C) 2004-2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_AVR32_PROCESSOR_H
#define __ASM_AVR32_PROCESSOR_H

#include <asm/page.h>
#include <asm/cache.h>

#define TASK_SIZE	0x80000000

#ifndef __ASSEMBLY__

static inline void *current_text_addr(void)
{
	register void *pc asm("pc");
	return pc;
}

enum arch_type {
	ARCH_AVR32A,
	ARCH_AVR32B,
	ARCH_MAX
};

enum cpu_type {
	CPU_MORGAN,
	CPU_AT32AP,
	CPU_MAX
};

enum tlb_config {
	TLB_NONE,
	TLB_SPLIT,
	TLB_UNIFIED,
	TLB_INVALID
};

struct avr32_cpuinfo {
	struct clk *clk;
	unsigned long loops_per_jiffy;
	enum arch_type arch_type;
	enum cpu_type cpu_type;
	unsigned short arch_revision;
	unsigned short cpu_revision;
	enum tlb_config tlb_config;

	struct cache_info icache;
	struct cache_info dcache;
};

extern struct avr32_cpuinfo boot_cpu_data;

#ifdef CONFIG_SMP
extern struct avr32_cpuinfo cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]
#else
#define cpu_data (&boot_cpu_data)
#define current_cpu_data boot_cpu_data
#endif

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's
 */
#define TASK_UNMAPPED_BASE	(PAGE_ALIGN(TASK_SIZE / 3))

#define cpu_relax()		barrier()
#define cpu_sync_pipeline()	asm volatile("sub pc, -2" : : : "memory")

struct cpu_context {
	unsigned long sr;
	unsigned long pc;
	unsigned long ksp;	/* Kernel stack pointer */
	unsigned long r7;
	unsigned long r6;
	unsigned long r5;
	unsigned long r4;
	unsigned long r3;
	unsigned long r2;
	unsigned long r1;
	unsigned long r0;
};

/* This struct contains the CPU context as stored by switch_to() */
struct thread_struct {
	struct cpu_context cpu_context;
	unsigned long single_step_addr;
	u16 single_step_insn;
};

#define INIT_THREAD {						\
	.cpu_context = {					\
		.ksp = sizeof(init_stack) + (long)&init_stack,	\
	},							\
}

/*
 * Do necessary setup to start up a newly executed thread.
 */
#define start_thread(regs, new_pc, new_sp)	 \
	do {					 \
		set_fs(USER_DS);		 \
		memset(regs, 0, sizeof(*regs));	 \
		regs->sr = MODE_USER;		 \
		regs->pc = new_pc & ~1;		 \
		regs->sp = new_sp;		 \
	} while(0)

struct task_struct;

/* Free all resources held by a thread */
extern void release_thread(struct task_struct *);

/* Create a kernel thread without removing it from tasklists */
extern int kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);

/* Prepare to copy thread state - unlazy all lazy status */
#define prepare_to_copy(tsk) do { } while(0)

/* Return saved PC of a blocked thread */
#define thread_saved_pc(tsk)    ((tsk)->thread.cpu_context.pc)

struct pt_regs;
void show_trace(struct task_struct *task, unsigned long *stack,
		struct pt_regs *regs);

extern unsigned long get_wchan(struct task_struct *p);

#define KSTK_EIP(tsk)	((tsk)->thread.cpu_context.pc)
#define KSTK_ESP(tsk)	((tsk)->thread.cpu_context.ksp)

#define ARCH_HAS_PREFETCH

static inline void prefetch(const void *x)
{
	const char *c = x;
	asm volatile("pref %0" : : "r"(c));
}
#define PREFETCH_STRIDE	L1_CACHE_BYTES

#endif /* __ASSEMBLY__ */

#endif /* __ASM_AVR32_PROCESSOR_H */
