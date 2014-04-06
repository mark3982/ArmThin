#ifndef MAIN_H
#define MAIN_H
#include "stdtypes.h"
#include "config.h"
#include "vmm.h"
#include "kheap.h"
#include "dbgout.h"

#define ARM4_XRQ_RESET   0x00
#define ARM4_XRQ_UNDEF   0x01
#define ARM4_XRQ_SWINT   0x02
#define ARM4_XRQ_ABRTP   0x03
#define ARM4_XRQ_ABRTD   0x04
#define ARM4_XRQ_RESV1   0x05
#define ARM4_XRQ_IRQ     0x06
#define ARM4_XRQ_FIQ     0x07

#define ARM4_MODE_USER   0x10
#define ARM4_MODE_FIQ	 0x11
#define ARM4_MODE_IRQ	 0x12
#define ARM4_MODE_SUPER  0x13
#define ARM4_MODE_ABORT	 0x17
#define ARM4_MODE_UNDEF  0x1b
#define ARM4_MODE_SYS    0x1f
#define ARM4_MODE_MON    0x16

#define CTRL_ENABLE			0x80
#define CTRL_MODE_FREE		0x00
#define CTRL_MODE_PERIODIC	0x40
#define CTRL_INT_ENABLE		(1<<5)
#define CTRL_DIV_NONE		0x00
#define CTRL_DIV_16			0x04
#define CTRL_DIV_256		0x08
#define CTRL_SIZE_32		0x02
#define CTRL_ONESHOT		0x01

#define REG_LOAD		0x00
#define REG_VALUE		0x01
#define REG_CTRL		0x02
#define REG_INTCLR		0x03
#define REG_INTSTAT		0x04
#define REG_INTMASK		0x05
#define REG_BGLOAD		0x06

#define PIC_IRQ_STATUS			0x0
#define PIC_IRQ_RAWSTAT			0x1
#define PIC_IRQ_ENABLESET		0x2
#define PIC_IRQ_ENABLECLR		0x3
#define PIC_INT_SOFTSET			0x4
#define PIC_INT_SOFTCLR			0x5

#define PIC_FIQ_STATUS			8
#define PIC_FIQ_RAWSTAT			9
#define PIC_FIQ_ENABLESET		10
#define PIC_FIQ_ENABLECLR		11

#define PANIC(msg) kprintf("PANIC %s LINE %x [%s]\n", __FILE__, __LINE__, msg); stackprinter(); for (;;)
#define ASSERTPANIC(cond, msg) if (!(cond)) { PANIC(msg); }

#define KEXP_TOPSWI \
	uint32			lr; \
	asm("mov sp, %[ps]" : : [ps]"i" (KSTACKEXC)); \
	asm("push {lr}"); \
	asm("push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}"); \
	asm("mov %[ps], lr" : [ps]"=r" (lr));	

#define KEXP_BOTSWI \
	asm("pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}"); \
	asm("LDM sp!, {pc}^")
		 
#define KEXP_TOP3 \
	uint32			lr; \
	asm("mov sp, %[ps]" : : [ps]"i" (KSTACKEXC)); \
	asm("sub lr, lr, #4"); \
	asm("push {lr}"); \
	asm("push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}"); \
	asm("mrs r0, spsr"); \
	asm("push {r0}"); \
	asm("mov %[ps], lr" : [ps]"=r" (lr));
	
#define KEXP_BOT3 \
	asm("pop {r0}"); \
	asm("msr spsr, r0"); \
	asm("pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}"); \
	asm("LDM sp!, {pc}^")

#define KSWI_WAKEUP				100
#define KSWI_SLEEP				101
#define KSWI_YEILD				102
	
#define KTHREAD_SLEEPING		0x1
#define KTHREAD_WAKEUP			0x2
	
typedef struct _KTHREAD {
	struct _KTHREAD		*next;
	struct _KTHREAD		*prev;
	
	uint32				timeout;			/* when to wakeup */
	uint8				flags;
	uint32				r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, cpsr, pc;
} KTHREAD;

typedef struct _KPROCESS {
	struct _KPROCESS	*next;
	struct _KPROCESS	*prev;
	
	KVMMTABLE			vmm;
	KTHREAD				*threads;
} KPROCESS;

typedef struct _KSTATE {
	/* new process/thread support */
	KPROCESS		*procs;
	KPROCESS		*cproc;
	KTHREAD			*cthread;
	
	/* physical and heap memory management */
	KHEAPBM			hphy;			/* kernel physical page heap */
	KHEAPBM			hchk;			/* data chunk heap */
	
	/* virtual memory management */
	KVMMTABLE		vmm;			/* kernel virtual memory map */
	uint32			vmm_ucte;		/* unused coarse table entries */
	KSTACK			tstack;			/* 1K table stack */
	uint32			*vmm_rev;		/* reverse map */
} KSTATE;

void stackprinter();

void* kmalloc(uint32 size);
void kfree(void *ptr);
#endif