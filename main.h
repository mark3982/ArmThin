#ifndef MAIN_H
#define MAIN_H
#include "stdtypes.h"
#include "config.h"
#include "vmm.h"
#include "corelib/kheap.h"
#include "corelib/rb.h"
#include "corelib/linklist.h"
#include "atomic.h"
#include "ds_mla.h"

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

#define GETCS() kboardGetCPUState()
#define GETKS() kboardGetCPUState()->ks

#define KSWI_WAKEUP				100
#define KSWI_SLEEP				101
#define KSWI_YIELD				102
#define KSWI_GETTICKPERSECOND	103
#define KSWI_KERNELMSG			104
#define KSWI_VALLOC				105
#define KSWI_VFREE				106
#define KSWI_TERMPROCESS		107
#define KSWI_TERMTHREAD			108
#define KSWI_TKMALLOC			109
#define KSWI_TKADDTHREAD		110
#define KSWI_TKSHAREMEM			111
#define KSWI_SIGNAL				112		/* same as KSWI_WAKEUP but issues signal also */
#define KSWI_GETSIGNAL			113
#define KSWI_GETSIGNALS			114
#define KSWI_GETOSTICKS			115
#define KSWI_GETVIRTREF			116
#define KSWI_GETPAGESIZE		117
#define KSWI_REQPHYMEMMAP		118
#define KSWI_SWITCHTO			119

#define KTHREAD_SLEEPING			0x01
#define KTHREAD_WAKEUP				0x02
#define KTHREAD_KIDLE				0x04
#define KTHREAD_DEAD				0x08
#define KTHREAD_WAKINGUPKTHREAD		0x10

#define KMSG_SENDMESSAGE		0
#define KMSG_SENDFAILED			1
#define KMSG_THREADMESSAGE		2
#define KMSG_REQSHARED			3
#define KMSG_ACPSHARED			4
#define KMSG_CREATETHREAD		5
#define KMSG_REGSERVICE			6
#define KMSG_ENUMSERVICE		7
#define KMSG_REGSERVICEFAILD	8
#define KMSG_REGSERVICEOK		9
#define KMSG_ENUMSERVICEREPLY	10
#define KMSG_REQSHAREDFAIL		11
#define KMSG_REQSHAREDOK		12
#define KMSG_ACPSHAREDFAIL		13
#define KMSG_ACPSHAREDREQUESTOR	14
#define KMSG_ACPSHAREDACCEPTOR	15

#define KSERVICE_DIRECTORY		0
#define KSERVICE_LOGGING		1

#define KPROCESS_DEAD			0x1
#define KPROCESS_SYSTEM			0x2
	
typedef struct _MWSR {
	LL				*deallocw;
	uintptr			*dealloc;
	uint32			max;
	KATOMIC_CCLOCK	lock;
} MWSR;

typedef struct _MWSRGLA_BLOCK {
	/* LL compatible */
	struct _MWSRGLA_BLOCK	*next;
	struct _MWSRGLA_BLOCK	*prev;
	
	uint32					used;
	uint32					flags;
	uint32					max;
	uintptr					slots[];
} MWSRGLA_BLOCK;

typedef struct _MWSRGLA {
	uint32					dmax;
	MWSRGLA_BLOCK			*fblock;
	KATOMIC_CCLOCK			lock;			/* cpu lock */
	KATOMIC_CCLOCK			tlock;			/* thread lock */
} MWSRGLA;
	
struct _KPROCESS;
struct _ERH;
	
typedef struct _KTHREAD {
	struct _KTHREAD		*next;
	struct _KTHREAD		*prev;

	/* debugging */
	char				*dbgname;
	
	/* signals */
	MLA					signals;
	
	/* thread kernel communication */
	struct _ERH			krx;			/* kernel server thread address */
	struct _ERH			ktx;			/* kernel server thread address */
	void				*urx;			/* thread space address */
	void				*utx;			/* thread space address */
	
	/* offering memory to share */
	uintptr				shreq_memoff;	/* memory offset */
	uintptr				shreq_pcnt;		/* page count */
	uintptr				shreq_rid;		/* request id */
	
	/* thread control */
	uint32				timeout;			/* when to wakeup */
	uint8				flags;
	uint32				locked;				/* locked */
	
	struct _KPROCESS	*proc;
	
	/* thread register state */
	uint32				r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, cpsr, pc;
} KTHREAD;

typedef struct _KPROCESS {
	struct _KPROCESS	*next;
	struct _KPROCESS	*prev;
	
	KVMMTABLE			vmm;
	KTHREAD				*threads;
	
	uint32				flags;
} KPROCESS;

typedef struct _K32PAIR {
	
} K32PAIR;

typedef struct _KSTATE {
	/* new process/thread support */
	KPROCESS					*procs;
	KTHREAD						*idleth;
	KPROCESS					*idleproc;
	KATOMIC_CCLOCK				schedlock;
	KSTACK						runnable;
	uint32						tswcycle;
	
	/* profiling stuff for scheduler (DEBUG STUFF) */
	uint32						tmpsum;
	uint32						tmpcnt;
	uint32						tmplow;
	
	/* kernel thread dealloc */
	MWSR						dealloc;
	
	/* system service registration (kthread uses this) */
	uintptr						ssr_proc[4];
	uintptr						ssr_th[4];
	
	/* kernel thread signal */
	MWSRGLA						ktsignal;
	
	/* restart catch (or another cpu starting) */
	uint32						rescatch;
	
	uint32						holdcpus;
	
	/* board state */
	void						*bif;
	
	/* ring buffer (IPC) */
	KPROCESS					*kservproc;
	KTHREAD						*kservthread;
	
	/* physical and heap memory management */
	KHEAPBM			hphy;			/* kernel physical page heap */
	KHEAPBM			hchk;			/* data chunk heap */
	
	/* time management */
	uint32			ctime;
	uint32			tpers;			/* ticks per second */
	
	/* virtual memory management */
	KVMMTABLE		vmm;			/* kernel virtual memory map */
	uint32			vmm_ucte;		/* unused coarse table entries */
	KSTACK			tstack;			/* 1K table stack */
	uint32			*vmm_rev;		/* reverse map */
	KATOMIC_CCLOCK	revlock;
} KSTATE;

typedef struct _KCPUSTATE {
	uintptr			excstack;
	KSTATE			*ks;
	KPROCESS		*cproc;
	KTHREAD			*cthread;
} KCPUSTATE;

void stackprinter();

void* kmalloc(uint32 size);
void kfree(void *ptr);

int kboardCheckAndClearTimerINT();
void kboardPrePagingInit();
void kboardPostPagingInit();
uint32 kboardGetTimerTick();
KCPUSTATE *kboardGetCPUState();				/* CPU local data and KERNEL local data */
#endif
