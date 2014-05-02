#include "stdtypes.h"
#include "main.h"
#include "kheap.h"
#include "vmm.h"
#include "kmod.h"
#include "atomic.h"
#include "ds_mla.h"

extern uint8 _BOI;
extern uint8 _EOI;

void start(void);

/*
	@sdescription:	The entry point for all CPUs controlled by this kernel.
    @devnote:		This could be non-standard behavior, but as long as 
					this resides at the top of this source and it is the
					first file used in the linking process (according to
					alphanumerical ordering) this code will start at the
					beginning of the .text section.
*/
void __attribute__((naked)) __attribute__((section(".boot"))) entry() {
	/* branch to board code */
	asm volatile ("b boardEntry");
}

/*
	@sdescription:		Sets byte of memory at specified location to
						value specified of size given.
*/
void memset(void *p, uint8 v, uintptr sz) {
	uint8 volatile		*_p;
	uintptr				x;
	
	_p = (uint8*)p;
	
	for (x = 0; x < sz; ++x) {
		_p[x] = v;
	}
	
	return;
}

/*
	@sdescription:		Primary kernel heap deallocation routine.
*/
void kfree(void *ptr) {
	KSTATE			*ks;
	
	ks = GETKS();

	k_heapBMFree(&ks->hchk, ptr);
}

/*
	@sdescription:		Primary kernel heap allocation routine.
*/
void* kmalloc(uint32 size) {
	void			*ptr;
	KSTATE			*ks;
	uint32			_size;	
	
	ks = GETKS();
	
	/* attempt first allocation try (will fail on very first) */
	ptr = k_heapBMAlloc(&ks->hchk, size);
	
	/* try adding more memory if failed */
	if (!ptr) {
		if (size < (KCHKMINBLOCKSZ * KPHYPAGESIZE) / 2) {
			/* we need to allocate blocks at least this size */
			_size = KCHKMINBLOCKSZ;
		} else {
			/* this is bigger than KCHKMINBLOCKSZ, so lets double it to be safe */
			/* round up allocation to use all the blocks taken */
			_size = size * 2;
			_size = (_size / KPHYPAGESIZE) * KPHYPAGESIZE < _size ? _size / KPHYPAGESIZE + 1 : _size / KPHYPAGESIZE;
			//_size = _size * KPHYPAGESIZE;
		}
		
		kprintf("EXTENDING KMALLOC HEAP _size:%x\n", _size);
		if (!kvmm2_allocregion(&ks->vmm, _size, 0, KMEMSIZE, TLB_C_AP_PRIVACCESS, (uintptr*)&ptr)) {
			/* no more physical pages */
			PANIC("KMALLOC EXTEND FAIL");
			return 0;
		}
		/* TODO: need to allocate virtual memory!! */
		//kserdbg_puts("WARNING: UNIMPLEMENTED/UNFINISHED ALLOC FOR KMALLOC NEW BLOCK\n");

		/* try allocation once more, should succeed */
		k_heapBMAddBlock(&ks->hchk, (uintptr)ptr, _size * KPHYPAGESIZE, KCHKHEAPBSIZE);
		ptr = k_heapBMAlloc(&ks->hchk, size);
		if (!ptr) {
			PANIC("KMALLOC SECOND FAIL");
		}
	}
	
	return ptr;
}
		
/*
	@sdescription:		Get CPSR of current CPU.
*/
uint32 arm4_cpsrget()
{
    uint32      r;
    
    asm("mrs %[ps], cpsr" : [ps]"=r" (r));
    return r;
}

/*
	@sdescription:		Get SPSR of current CPU.
*/
uint32 arm4_spsrget()
{
    uint32      r;
    
    asm("mrs %[ps], spsr" : [ps]"=r" (r));
    return r;
}

/*
	@sdescription:		Set CPSR of current CPU.
*/
void arm4_cpsrset(uint32 r)
{
    asm("msr cpsr, %[ps]" : : [ps]"r" (r));
}
	
/*
	@sdescription:		Enable FIQ
*/
void arm4_xrqenable_fiq()
{
    arm4_cpsrset(arm4_cpsrget() & ~(1 << 6));
}

/*
	@sdescription:		Enable IRQ
*/
void arm4_xrqenable_irq()
{
    arm4_cpsrset(arm4_cpsrget() & ~(1 << 7));
}

/*
	@sdescription:		Set TTBR0 table address.
*/
void arm4_tlbset0(uint32 base) {
	asm("mcr p15, 0, %[tlb], c2, c0, 0" : : [tlb]"r" (base));
}

/*
	@sdescription:		Set TTBR1 table address.
*/
void arm4_tlbset1(uint32 base) {
	asm("mcr p15, 0, %[tlb], c2, c0, 1" : : [tlb]"r" (base));
}

/*
	@sdescription:		Get TTBR1 table address.
*/
uint32 arm4_tlbget1() {
	uint32			base;
	asm("mrc p15, 0, %[tlb], c2, c0, 1" : [tlb]"=r" (base));
	return base;
}

/*
	@sdescription:		Set TLB mode. TTBR0 and TTBR1 division.
*/
void arm4_tlbsetmode(uint32 val) {
	asm("mcr p15, 0, %[tlb], c2, c0, 2" : : [tlb]"r" (val));
}

/*
	@sdescription:		Sets exception/vector table base address.
*/
void arm4_setvecbase(uint32 val) {
	asm("mcr p15, 0, %[val], c12, c0, 0" : : [val]"r" (val));
}

/*
	@sdescription:		Sets ARM domain register.
*/
void arm4_tlbsetdom(uint32 val) {
	asm("mcr p15, 0, %[val], c3, c0, 0" : : [val]"r" (val));
}

/*
	@sdescription:		Gets ARM TLB control register.
*/
uint32 arm4_tlbgetctrl() {
	uint32			ctrl;
	asm("mrc p15, 0, %[ctrl], c1, c0, 0" : [ctrl]"=r" (ctrl));
	return ctrl;
}

/*
	@sdescription:		Sets ARM TLB control register.
*/
void arm4_tlbsetctrl(uint32 ctrl) {
	asm("mcr p15, 0, %[ctrl], c1, c0, 0" : : [ctrl]"r" (ctrl));
}

/*
	@sdescription:		Allocates single page from the physical page heap.
*/
void* kpalloc(uint32 size) {
	KSTATE			*ks;
	
	ks = GETKS();
	return k_heapBMAlloc(&ks->hphy, size);
}

/*
	@sdescription:		Frees a single page back to the physical page heap.
*/
void kpfree(void *ptr) {
	KSTATE			*ks;
	
	ks = GETKS();
	k_heapBMFree(&ks->hphy, ptr);
}

/*
	@sdescription:		Dumps thread state. Hardly ever used.
*/
void kdumpthreadinfo(KTHREAD *th) {
	kprintf("r0:%x\tr1:%x\tr2:%x\tr3:%x\n", th->r0, th->r1, th->r2, th->r3);
	kprintf("r4:%x\tr5:%x\tr6:%x\tr7:%x\n", th->r4, th->r5, th->r6, th->r7);
	kprintf("r8:%x\tr9:%x\tr10:%x\tr11:%x\n", th->r8, th->r9, th->r10, th->r11);
	kprintf("r12:%x\tsp:%x\tlr:%x\tcpsr:%x\n", th->r12, th->sp, th->lr, th->cpsr);
}

/*
	@sdescription:		Allows a CPU to save the current thread and load
						the next thread.
*/
void ksched() {
	KSTATE			*ks;
	KTHREAD			*kt;
	uint32			__lr, __sp, __spsr;
	uintptr			page;
	uint32			tmp0, tmp1;
	KPROCESS		*proc;
	KTHREAD			*th;
	KCPUSTATE		*cs;
	uint32			*stk;
	
	cs = GETCS();
	ks = GETKS();
	
	stk = (uint32*)cs->excstack;

	/* 
		i use wfi for qemu because of the way it handles multiple cpus; if
		i dont let it go to sleep it just sits there spinning making the
		other cpus wait before they execute
	*/
	KCCENTER(&ks->schedlock);
	if (ks->holdcpus == 1) {
		for (;;) {
			kprintf("WARNING cpu-holding-lock:%x\n", ks->schedlock.lock);
			for (;;);
		}
	}
	
	ks->holdcpus = 1;
	
	//kprintf("SCHED  ks:%x\n", ks);
	
	//kprintf("SCHED cpu:%x ks:%x\n", boardGetCPUID(), ks);
	
	/* save previous thread, if any */
	if (cs->cthread) {
		/*
			WARNING: check for idle thread...
			might need to have one idle thread PER CPU...
		*/
		//kprintf("SAVING\n");
		kt = cs->cthread;
		kt->pc = stk[-1];
		kt->r12 = stk[-2];
		kt->r11 = stk[-3];
		kt->r10 = stk[-4];
		kt->r9 = stk[-5];
		kt->r8 = stk[-6];
		kt->r7 = stk[-7];
		kt->r6 = stk[-8];
		kt->r5 = stk[-9];
		kt->r4 = stk[-10];
		kt->r3 = stk[-11];
		kt->r2 = stk[-12];
		kt->r1 = stk[-13];
		kt->r0 = stk[-14];
		kt->cpsr = stk[-15];
		/* switch to system mode get hidden registers then switch back */
		asm volatile ("mrs %[tmp0], cpsr \n\
			 mov %[tmp1], %[tmp0] \n\
			 bic %[tmp0], %[tmp0], #0x1f \n\
			 orr %[tmp0], %[tmp0], #0x1f \n\
			 msr cpsr, %[tmp0] \n\
			 mov %[sp], sp \n\
			 mov %[lr], lr \n\
			 msr cpsr, %[tmp1] \n\
			 " : [tmp0]"+r" (tmp0), [tmp1]"+r" (tmp1), [sp]"=r" (__sp), [lr]"=r" (__lr));
		kt->sp = __sp;
		kt->lr = __lr;
		kt->locked = 0;			/* unlock thread so other cpus can grab it */
	}
	
	/* build runnable stack if needed */
	if (kstack_empty(&ks->runnable)) {
		//kprintf("BUILDING RUNNABLE\n");
		/* fill the stack with runnable threads */
		for (proc = ks->procs; proc; proc = proc->next) {
			for (th = proc->threads; th; th = th->next) {
				if (th->flags & KTHREAD_KIDLE) {
					continue;
				}				
				
				if (th->locked) {
					continue;
				}
				
				/* only wakeup if it is sleeping */
				if (th->flags & KTHREAD_SLEEPING) {
					//kprintf("SLEEPER name:%s flags:%x\n", th->dbgname ? th->dbgname : "<unknown>", th->flags);
					
					if (th->timeout > 0 && (ks->ctime > th->timeout)) {
						//kprintf("WOKE UP (timeout) %x %x\n", th, th->timeout);
						/* wake up thread if passed timeout */
						th->flags &= ~KTHREAD_SLEEPING;
						th->r0 = 0;
					}
					
					/* wakeup thread is set to be woken up */
					if (th->flags & KTHREAD_WAKEUP) {
						kprintf("WOKE UP (signal) thread:%x dbgname:%s\n", th, th->dbgname ? th->dbgname : "<unknown>");
						th->flags &= ~(KTHREAD_WAKEUP | KTHREAD_SLEEPING);
						th->r0 = ks->ctime - th->timeout;
					}
				}
				
				//if (th->flags & KTHREAD_SLEEPING) {
					//kprintf("SLEEPING timeleft:%x timeout:%x ctime:%x name:%s\n", th->timeout - ks->ctime, th->timeout, ks->ctime, th->dbgname ? th->dbgname : "unknown");
				//}
				
				/* if NOT sleeping  */
				if (!(th->flags & KTHREAD_SLEEPING)) {
					//kprintf("   ADDED %x:%s\n", th, th->dbgname);
					kstack_push(&ks->runnable, (uintptr)th);
				}
			}
		}
		
		/* only idle thread left */
		if (kstack_empty(&ks->runnable)) {
			//kprintf("    ADDED IDLE\n");
			kstack_push(&ks->runnable, (uintptr)ks->idleth);
		}
	}
	
	kstack_pop(&ks->runnable, (uintptr*)&cs->cthread);
	cs->cproc = cs->cthread->proc;
	//kprintf("---------------------------------POPPING THREAD %x %s\n", cs->cproc, cs->cthread->dbgname);
	
	/* hopefully we got something or the system should deadlock */
	kt = cs->cthread;
	/*
		load registers
	*/
	stk[-1] = kt->pc;
	stk[-2] = kt->r12;
	stk[-3] = kt->r11;
	stk[-4] = kt->r10;
	stk[-5] = kt->r9;
	stk[-6] = kt->r8;
	stk[-7] = kt->r7;
	stk[-8] = kt->r6;
	stk[-9] = kt->r5;
	stk[-10] = kt->r4;
	stk[-11] = kt->r3;
	stk[-12] = kt->r2;
	stk[-13] = kt->r1;
	stk[-14] = kt->r0;
	stk[-15] = kt->cpsr;
	/* switch into system mode restore hidden registers then switch back */
	asm volatile (
		"mrs %[tmp0], cpsr \n\
		 mov %[tmp1], %[tmp0] \n\
		 bic %[tmp0], %[tmp0], #0x1f \n\
		 orr %[tmp0], %[tmp0], #0x1f \n\
		 msr cpsr, %[tmp0] \n\
		 mov sp, %[sp] \n\
		 mov lr, %[lr] \n\
		 msr cpsr, %[tmp1] \n\
		 " : [tmp0]"+r" (tmp0), [tmp1]"+r" (tmp1) : [sp]"r" (kt->sp), [lr]"r" (kt->lr));
	/* set TLB table for user space (it can be zero for kernel) */
	kvmm2_getphy(&ks->vmm, (uintptr)kt->proc->vmm.table, &page);
	arm4_tlbset1(page);
	kt->locked = 1;			/* lock so no other cpu tries to run it */
	/* 
		Invalidate all unlocked entries...
		
		..according to the manual there may be a better way to invalidate,
		only some entries per process. But, for now this should work.
		
		If you do not do this then the TLB does not flush and old entries
		from the previous process will still be in the TLB cache.
	*/
	asm("mcr p15, #0, r0, c8, c7, #0");
	
	//kprintf("SWITCH-TO thread:%x cpsr:%x fp:%x sp:%x pc:%x dbgname:%s\n", kt, kt->cpsr, kt->r11, kt->sp, kt->pc, kt->dbgname);
	
	//uint32			*p;
	
	//if (!kvmm2_getphy(&cs->cproc->vmm, 0x90000000, (uintptr*)&p)) {
		//kprintf("NO STACK EXISTS??\n");
	//} else {
		//kprintf("writing to stack..%x\n", kt->sp);
		//((uint32*)kt->sp)[-1] = 0xbb;
	//}
	
	//if (kvmm2_getphy(&cs->cproc->vmm, 0x80000000, (uintptr*)&p)) {
	//	uint32			x;
		
		//kprintf("CODE PAGE :%x\n", p);
		
		//p = (uint32*)(0x80000000);

		//stk[-1] = 0x80000800;
	
		//for (x = 0; x < 1024; ++x) {
		//	p[x] = 0xeafffffe;
		//}
	
	//} else {
		//kprintf("CODE PAGE????\n");
	//}
	
	ks->holdcpus = 0;
	
	KCCEXIT(&ks->schedlock);
}

static void kprocfree__walkercb(uintptr v, uintptr p) {
	KSTATE					*ks;
	uint32					r0;
	KCPUSTATE				*cs;
	
	cs = GETCS();
	ks = GETKS();

	/* just unmap it to zero the entry for safety */
	kvmm2_unmap(&cs->cproc->vmm, v, 0);
	/* figure out if it is alloc'ed or shared */
	r0 = kvmm2_revget(p, 1);
	switch (r0) {
		case 1:
			/* no references left (decrement to zero) */
			kprintf("    no references (but alloced) %x:%x\n", v, p);
			kvmm2_revdec(p);
			k_heapBMFree(&ks->hphy, (void*)p);
			break;
		case 0:
			/* do nothing (only mapped not alloced) */
			break;
		default:
			kprintf("     decrement ref cnt for %x:%x\n", v, p);
			/* decrement reference count */
			kvmm2_revdec(p);
			break;
	}
}

/*
	@sdescription:	Not sure if this is used anymore..
*/
int kprocfree(KPROCESS *proc) {
	kprintf("kprocfree...\n");
	/* walk entries and free them */
	kvmm2_walkentries(&proc->vmm, &kprocfree__walkercb);
	
	return 1;
}

/*
	@sdescription:		Gets one item from extending array.
*/
int mwsrgla_get(MWSRGLA *mwsrgla, MWSRGLA_BLOCK **cblock, uint32 *index, uintptr *val) {
	uint32				x;
	MWSRGLA_BLOCK		*b;
	
	katomic_ccenter_yield(&mwsrgla->tlock);
	
	if (!*cblock) {
		*cblock = mwsrgla->fblock;
		*index = 0;
	}
	
	b = *cblock;
	
	if (*index > b->max) {
		PANIC("*index > mwsrgla->dmax");
		katomic_ccexit_yield(&mwsrgla->tlock);
		return 0;
	}
	
	/* bad mojo but its a lot simplier to read and optimize for i think */
	goto skipinside;
	
	for (b = *cblock; b; b = b->next) {
		*index = 0;
		
		skipinside:					/* loop entry */
		if (b->used == 0) {
			continue;
		}
		
		for (x = *index; x < b->max; ++x) {
			if (b->slots[x]) {
				*index = x + 1;
				*val = b->slots[x];
				*cblock = b;
				b->used--;				/* update used count */
				b->slots[x] = 0;		/* free slot */
				katomic_ccexit_yield(&mwsrgla->tlock);
				return 1;
			}
		}
	}
	
	*cblock = 0;
	*index = 0;
	katomic_ccexit_yield(&mwsrgla->tlock);
	return 0;
}

/*
	@sdescription:	For CPU to adds one item to extending array.
*/
int mwsrgla_add(MWSRGLA *mwsrgla, uintptr val) {
	MWSRGLA_BLOCK			*b;
	uint32					x;
	
	KCCENTER(&mwsrgla->lock);
	for (b = mwsrgla->fblock; b; b = b->next) {
		if (b->used < b->max) {
			for (x = 0; x < b->max; ++x) {
				if (!b->slots[x]) {
					b->slots[x] = val;
					++b->used;
					KCCEXIT(&mwsrgla->lock);
					return 1;
				}
			}
		}
	}
	/* we need to add another block */
	b = (MWSRGLA_BLOCK*)kmalloc(sizeof(MWSRGLA_BLOCK) + sizeof(uintptr) * mwsrgla->dmax);
	b->max = mwsrgla->dmax;
	b->flags = 0;
	b->used = 0;
	ll_add((void**)&mwsrgla->fblock, b);
	/* store value */
	++b->used;
	b->slots[0] = val;
	KCCEXIT(&mwsrgla->lock);
}

/*
	@sdescription:	Initializes the dynamically extending container.
*/
int mwsrgla_init(MWSRGLA *mwsrgla, uint32 dmax) {
	uint32			x;

	mwsrgla->dmax = dmax;
	mwsrgla->fblock = (MWSRGLA_BLOCK*)kmalloc(sizeof(MWSRGLA_BLOCK) + sizeof(uintptr) * dmax);
	if (!mwsrgla->fblock) {
		PANIC("MWSRGLA_INIT FAILED WITH KMALLOC");
		return 0;
	}
	mwsrgla->fblock->flags = 0;
	mwsrgla->fblock->next = 0;
	mwsrgla->fblock->prev = 0;
	mwsrgla->fblock->max = dmax;
	for (x = 0; x < dmax; ++x) {
		mwsrgla->fblock->slots[x] = 0;
	}
	
	return 1;
}

/*	
	@sdescription:	Initialize to be deallocated pointer container.
*/
int mwsr_init(MWSR *mwsr, uint32 max) {
	memset(mwsr, 0, sizeof(MWSR));
	mwsr->max = max;
	mwsr->dealloc = (uintptr*)kmalloc(sizeof(uintptr) * max);
	if (!mwsr->dealloc) {
		PANIC("MWSR->DEALLOC NULL ON ALLOC");
		return 0;
	}
	mwsr->deallocw = 0;			/* just to be safe */
	return 1;
}

/*
	@sdescription:	Used by kernel thread to get
					one item that needs to be allocated.
*/
int mwsr_getone(MWSR *mwsr, uintptr *v) {
	uint32			x;

	for (x = 0; x < mwsr->max; ++x) {
		if (mwsr->dealloc[x]) {
			*v = mwsr->dealloc[x];
			mwsr->dealloc[x] = 0;
			return 1;
		}
	}
	*v = 0;
	return 0;
}

/*
	@sdescription:	Used by a CPU to place an item to be deallocated later.
*/
int mwsr_add(MWSR *mwsr,  uintptr v) {
	LL			*ll, *_ll;
	uint32		x;
	
	KCCENTER(&mwsr->lock);
	
	for (x = 0; x < mwsr->max; ++x) {
		if (!mwsr->dealloc[x]) {
			mwsr->dealloc[x] = v;
		}
	}

	/* periodically we can dump these back into the 'dealloc' array */
	if (x >= mwsr->max) {
		/* link it into wait list */
		ll_add((void**)&mwsr->deallocw, (void*)v);
	} else {
		/* try to dump (some of) these in the dealloc list */
		ll = mwsr->deallocw;
		for (x = 0; ll && (x < mwsr->max); ++x) {
			if (!mwsr->dealloc[x]) {	/* free slot? */
				_ll = ll;				/* get handle on current (about to be removed) */
				ll = ll->next;			/* get next (ahead of time) [ll may be null] */
				ll_rem((void**)&mwsr->deallocw, (void*)_ll);
				/* put in last incase it gets grabbed before the above */
				mwsr->dealloc[x] = (uintptr)ll;
			}
		}										
	}
	
	KCCEXIT(&mwsr->lock);
	return 1;
}

/*
	@sdescription:		Provides locking to remove a process from the
						scheduler.
*/
static void kswi_termprocess() {
	KSTATE				*ks;
	KCPUSTATE			*cs;
	KPROCESS			*proc;
	KTHREAD				*th, *_th;
	
	ks = GETKS();
	cs = GETCS();

	/* unlink process and threads, but keep modify pointer so
	   scheduler can grab next thread */
	KCCENTER(&ks->schedlock);
	proc = cs->cproc;
	proc->flags |= KPROCESS_DEAD;
	
	/* add dead process to be deallocated */
	mwsr_add(&ks->dealloc, (uintptr)proc);
	
	/* should schedule a thread from the next process */
	ksched();
	/* restore pointer */
	th->next = _th;
	/* cycle through threads releasing resources */
	for (th = proc->threads; th; th = _th) {
		_th = th->next;
		/* add thread to either dealloc or deallocw */
		mwsr_add(&ks->dealloc, (uintptr)th);
		th->flags |= KTHREAD_DEAD;
	}
	/* free process resources */
	if (!kprocfree(proc)) {
		PANIC("ERROR-FREEING-PROC-RESOURCES");
	}
	/* release heap memory */
	kfree(proc);
	KCCEXIT(&ks->schedlock);;
}

/*
	@sdescription:		Provides locking needed to remove a thread from a process.
*/
static void kswi_termthread() {
	KPROCESS			*proc;
	KTHREAD				*th;
	KSTATE				*ks;
	KCPUSTATE			*cs;
	
	ks = GETKS();
	cs = GETCS();

	KCCENTER(&ks->schedlock);
	proc = cs->cproc;
	th = cs->cthread;
	/* should schedule a thread from the next process */
	ksched();
	/* now unlink and free thread resources */
	ll_rem((void**)&proc->threads, th);
	mwsr_add(&ks->dealloc, (uintptr)th);
	th->flags |= KTHREAD_DEAD;
	KCCEXIT(&ks->schedlock);
	return;
}

/*
	@sdescription:			Common entry of all exceptions. Provides a central
							place to handle exceptions.
*/
void k_exphandler(uint32 lr, uint32 type) {
	uint32 volatile	*t0mmio;
	uint8 volatile	*picmmio;
	uint32			swi;
	KSTATE			*ks;
	int				x;
	KTHREAD			*kt;
	uintptr			out, out2;
	uint32			r0, r1, r2;
	KPROCESS		*proc;
	KTHREAD			*th, *_th;
	KCPUSTATE		*cs;
	uint32 			*stk;
	LL				*ll;
	
	cs = GETCS();
	ks = GETKS();

	stk = (uint32*)cs->excstack;	
	
	//kprintf("@@@@@@@@@@@@@@@@@@@:%x\n", ks->hphy.fblock->size);
	//kserdbg_putc('H');
	//kserdbg_putc('\n');
	
	//asm("mov %[var], sp\n" : [var]"=r" (r0));
	//kprintf("SPSPSPSP:%x type:%x\n", r0, type);
	
	if (type == ARM4_XRQ_FIQ) {
		for (;;) {
			kprintf("PANIC: NOT EXPECTING FIQ\n");
		}
	}
	
	if (type == ARM4_XRQ_IRQ) {
		if (kboardCheckAndClearTimerINT()) {			
			/* update current time */
			r0 = kboardGetTimerTick();
			ks->ctime += r0;
			//kprintf("delta:%x ks->ctime:%x ks:%x\n", r0, ks->ctime, ks);
			//kprintf("IRQ: ctime:%x timer-tick:%x\n", ks->ctime, kboardGetTimerTick());
			ksched();
			
			r0 = kboardGetTimerTick() - r0;
			
			if (!ks->tmplow) {
				ks->tmplow = ~0;
			}
			
			if (r0 < ks->tmplow) {
				ks->tmplow = r0;
				kprintf("tmplow:%x\n", ks->tmplow);
			}
			
			if (r0 < 0xffff) {
				ks->tmpsum += r0;
				ks->tmpcnt++;
			}
			
			kprintf("total-ticks:%x\n", ks->tmpsum / ks->tmpcnt);
			/* go back through normal interrupt return process */
			return;
		}
		return;
	}
	
	/*
		Get SWI argument (index).
	*/
	if (type == ARM4_XRQ_SWINT) {
		swi = ((uint32*)((uintptr)lr - 4))[0] & 0xffff;
		
		//kprintf("SWI thread:%x name:%s code:%x lr:%x\n", cs->cthread, cs->cthread->dbgname, swi, lr);
		
		//stk[-14] = R0;
		//stk[-13] = R1;
		switch (swi) {
			case KSWI_TERMPROCESS:
				kswi_termprocess();
				break;
			case KSWI_TERMTHREAD:
				kswi_termthread();
				break;
			case KSWI_GETPAGESIZE:
				stk[-14] = KVIRPAGESIZE;
				break;
			case KSWI_VALLOC:
				/* allocate range of pages and store result in R0 */
				r0 = stk[-14];
				kprintf("KSWI_VALLOC r0:%x\n", r0);
				kvmm2_allocregion(&cs->cproc->vmm, r0, KMEMSIZE, 0, TLB_C_AP_FULLACCESS, &stk[-14]);
				kprintf("            stk[-14]:%x\n", stk[-14]);
				break;
			case KSWI_VFREE:
				/*
					We have to be careful here because the memory can be shared by two or more processes,
					which means we have to consult our reference table before actually handing the memory
					back to the physical memory manager.
					
					This value is incremented in two places:
						1. in the vmm's alloc function
						2. when memory is shared
						
					Otherwise, unless manually incremented, it should be zero which means we do not
					deallocate it because it was directly mapped.
					
					> 1 	shared (DO NOT FREE YET)							decrement it
					== 1	only reference, allocated through vmm function		decrement it
					0		mapped directly (not considered allocated)			nothing
					
				*/
				r0 = stk[-14];
				r1 = stk[-13];
				kprintf("KSWI_VFREE r0:%x r1:%x\n", r0, r1);
				for (x = 0; x < r1; ++x) {
					/* unmap (dont free.. yet) */
					kvmm2_getphy(&cs->cproc->vmm, r0 + x * 0x1000, &out);
					kvmm2_unmap(&cs->cproc->vmm, r0 + x * 0x1000, 0);
					r0 = kvmm2_revget(out, 1);
					switch (r0) {
						case 1:
							/* no references left (decrement to zero) */
							kprintf("    no references (but alloced) %x\n", r0 + x * 0x1000);
							kvmm2_revdec(out);
							k_heapBMFree(&ks->hphy, (void*)out);
							break;
						case 0:
							/* do nothing (only mapped not alloced) */
							break;
						default:
							kprintf("     decrement ref cnt for %x\n", r0 + x * 0x1000);
							/* decrement reference count */
							kvmm2_revdec(out);
							break;
					}
				}
				break;
			case KSWI_GETOSTICKS:
				stk[-14] = ks->ctime;
				break;
			case KSWI_GETSIGNALS:
				/* grabs up to 1024 signals at a time */
				break;
			case KSWI_GETSIGNAL:
				/* get one signal signal and return */
				stk[-14] = mla_get(&th->signals, &out, &out2);
				stk[-13] = out;			/* process */
				stk[-12] = out2;		/* signal */
				break;
			case KSWI_SIGNAL:
				r0 = stk[-14];		/* proc */
				r1 = stk[-13];		/* thread */
				r2 = stk[-12];		/* signal */

				stk[-14] = 0;		/* default failure code */
				for (proc = ks->procs; proc; proc = proc->next) {
					if ((uint32)proc == r0) {
						for (th = proc->threads; th; th = th->next) {
							if ((uint32)th == r1) {
								/* found it now add signal */
								mla_add(&th->signals, (uintptr)cs->cproc, r2);
								stk[-14] = 1;	/* change code to successful */
								break;
							}
						}
						break;
					}
				}
				break;
			case KSWI_WAKEUP:
				/* wake up thread function */
				r0 = stk[-14];
				r1 = stk[-13];
				
				for (proc = ks->procs; proc; proc = proc->next) {
					if ((uint32)proc == r0) {
						for (th = proc->threads; th; th = th->next) {
							if ((uint32)th == r1) {
								/* wake up thread */
								th->flags |= KTHREAD_WAKEUP;
								break;
							}
						}
						break;
					}
				}
				break;
			case KSWI_GETTICKPERSECOND:
				stk[-14] = ks->tpers;
				kprintf("GETTICKSPERSECOND name:%s\n", cs->cthread->dbgname);
				break;
			case KSWI_SLEEP:
				/* thread sleep function */
				r0 = stk[-14];
				
				//kprintf("SLEEPSYSCALL thread:%x duration:%x flags:%x name:%s\n", cs->cthread, r0, cs->cthread->flags, cs->cthread->dbgname ? cs->cthread->dbgname : "<unknown>");

				cs->cthread->flags |= KTHREAD_SLEEPING;
				if (r0 != 0) {
					//kprintf("SLEEP total:%x r0:%x ks->ctime:%x\n", r0 + ks->ctime, r0, ks->ctime);
					cs->cthread->timeout = r0 + ks->ctime;
				} else {
					cs->cthread->timeout = 0;
				}

				ksched();
				break;
			case KSWI_YIELD:
				ksched();
				break;
			case KSWI_TKADDTHREAD:
				/* TODO: check for privilege mode */
				r0 = stk[-14];
				r1 = stk[-13];
				kprintf("SWI ADDTHREAD proc:%x thread:%x\n", r0, r1);				
				KCCENTER(&ks->schedlock);
				ll_add((void**)&((KPROCESS*)r0)->threads, (void*)r1);
				KCCEXIT(&ks->schedlock);
				break;
			case KSWI_KERNELMSG:
				/* make sure the kthread wakes up */
				/* TODO: IF MULTIPLE KTHREADS THEN CONSIDER WAKING ONE UP OR IN SOME ORDER */
				ks->kservthread->flags |= KTHREAD_WAKEUP;
				if (cs->cthread->flags & KTHREAD_WAKINGUPKTHREAD) {
					/* prevention of DOS attack on service */
					break;
				}
				cs->cthread->flags |= KTHREAD_WAKINGUPKTHREAD;
				/* add signal for thread (internal locking per cpu) */
				mwsrgla_add(&ks->ktsignal, (uintptr)cs->cthread);
				kprintf("SYSCALL KERNELMSG\n");
				break;
			case KSWI_TKMALLOC:
				/* TODO: check for privilege mode */
				((uint32 volatile*)stk)[-14] = (uint32)kmalloc(stk[-14]);
				kprintf("KSWI_TKMALLOC %x\n", stk[-14]);
				break;
			case KSWI_TKSHAREMEM:
			{
				uint32				x;
				uintptr				out;
				uintptr				phy;
				uintptr				addr;
				uintptr				pcnt;
				KPROCESS			*acceptor;
				KPROCESS			*requestor;
				
				acceptor = (KPROCESS*)stk[-14];
				requestor = (KPROCESS*)stk[-13];
				addr = stk[-12];
				pcnt = stk[-11];
				
				printf("[tksharemem] acceptor:%x requestor:%x addr:%x pcnt:%x\n", acceptor, requestor, addr, pcnt);
				
				/* find free range for acceptor */
				if (!kvmm2_findregion(&acceptor->vmm, pcnt, KMEMSIZE, 0, 0, &out)) {
					((uint32 volatile*)stk)[-14] = 0;
					((uint32 volatile*)stk)[-13] = 0;
					printf("[tksharemem] failed to find region in acceptor\n");
					break;
				}
				
				printf("[tksharemem] mapping pages..\n");
				for (x = 0; x < pcnt; ++x) {
					kvmm2_getphy(&requestor->vmm, addr, &phy);
					kvmm2_mapsingle(&acceptor->vmm, out + x * 0x1000, phy, TLB_C_AP_FULLACCESS);
					printf("[tksharemem]      virtual:%x physical:%x\n", out + x * 0x1000, phy);
				}
				
				((uint32 volatile*)stk)[-14] = 1;
				((uint32 volatile*)stk)[-13] = out;
				printf("[tksharemem] operation completed\n");
				break;
			}
			default:
				break;
		}
		return;
	}
	
	if (type != ARM4_XRQ_IRQ && type != ARM4_XRQ_FIQ && type != ARM4_XRQ_SWINT) {
		/*
			Ensure, the exception return code is correctly handling LR with the
			correct offset. I am using the same return for everything except SWI, 
			which requires that LR not be offset before return.
		*/
		KTHREAD			*tmp;
		uint32			__sp, __lr;
		uint32			cpu;
		
		cpu = boardGetCPUID();
		
		tmp = cs->cthread;
		
		for (;;) {
			r0 = stk[-1];
			for (x = 0; x < 0xffffff; ++x);
			kprintf("!EXCEPTION pc:%x type:%x ks:%x cpu:%x ks->cthread:%x\n", r0, type, ks, cpu, cs->cthread);
			kprintf("   CPU:%x type:%x cproc:%x cthread:%x lr:%x dbgname:%s\n", cpu, type, cs->cproc, cs->cthread, lr, cs->cthread ? cs->cthread->dbgname : "$none$");
		
			asm volatile ("mrs %[tmp0], cpsr \n\
				 mov %[tmp1], %[tmp0] \n\
				 bic %[tmp0], %[tmp0], #0x1f \n\
				 orr %[tmp0], %[tmp0], #0x1f \n\
				 msr cpsr, %[tmp0] \n\
				 mov %[sp], sp \n\
				 mov %[lr], lr \n\
				 msr cpsr, %[tmp1] \n\
				 " : [tmp0]"+r" (r0), [tmp1]"+r" (r1), [sp]"=r" (__sp), [lr]"=r" (__lr));
			
			kprintf("    sp:%x lr:%x\n", __sp, __lr);
			
			r0 = stk[-1];
			r0 = ((uint32*)r0)[0];
			kprintf("inst:%x\n", r0);
			
			for (;;);
		}
		for (;;);
		if (lr >= (uintptr)&_BOI && lr <= (uintptr)&_EOI) {
			PANIC("CRITICAL FAILURE: exception in kernel image\n");
		}
		
		for (;;);
		
		ll_rem((void**)&cs->cproc->threads, cs->cthread);
		cs->cthread = cs->cthread->next;
		
		ksched();
		
		kdumpthreadinfo(tmp);
	}
	
	return;
}

/*
	@sdescription:		Installs handler for exception vector using the absolute
						address of the handler. Handler must be within 32MB of
						the exception table at memory location 0x0.
*/
void arm4_xrqinstall(uint32 ndx, void *addr) {
	char buf[32];
    uint32      *v;
    
    v = (uint32*)0x0;
	v[ndx] = 0xEA000000 | (((uintptr)addr - (8 + (4 * ndx))) >> 2);
}

typedef uint16 Elf32_Half;
typedef uint32 Elf32_Word;
typedef uint32 Elf32_Off;
typedef uint32 Elf32_Addr;

#define EI_NIDENT 16
typedef struct {
        unsigned char   e_ident[EI_NIDENT];
        Elf32_Half      e_type;
        Elf32_Half      e_machine;
        Elf32_Word      e_version;
        Elf32_Addr      e_entry;
        Elf32_Off       e_phoff;
        Elf32_Off       e_shoff;
        Elf32_Word      e_flags;
        Elf32_Half      e_ehsize;
        Elf32_Half      e_phentsize;
        Elf32_Half      e_phnum;
        Elf32_Half      e_shentsize;
        Elf32_Half      e_shnum;
        Elf32_Half      e_shtrndx;
} ELF32_EHDR;

#define EM_ARM			40

typedef struct {
       Elf32_Word      sh_name;
       Elf32_Word      sh_type;
       Elf32_Word      sh_flags;
       Elf32_Addr      sh_addr;
       Elf32_Off       sh_offset;
       Elf32_Word      sh_size;
       Elf32_Word      sh_link;
       Elf32_Word      sh_info;
       Elf32_Word      sh_addralign;
       Elf32_Word      sh_entsize;
} ELF32_SHDR;

/*
	@sdescription:		Loads an ELF32 image from kernel space into it's
						own userspace, and creates one thread that starts
						at the specified entry point.
*/
KTHREAD* kelfload(KPROCESS *proc, uintptr addr, uintptr sz) {
	ELF32_EHDR			*ehdr;
	ELF32_SHDR			*shdr;
	uint32				x, y;
	uintptr				page, oldpage;
	KSTATE				*ks;
	uint8				*fb;
	KTHREAD				*th;
	uintptr				out, out2;
	
	kprintf("@@ loading elf into memory space\n");
	
	ks = GETKS();
	
	kprintf("HERE\n");
		
	ehdr = (ELF32_EHDR*)addr;
	if (ehdr->e_machine != EM_ARM) {
		kprintf("kelfload: not ARM machine!\n");
		return 0;
	}
	
	if (ehdr->e_ident[4] != 0x1) {
		kprintf("kelfload: not ELF32 object\n");
		return 0;
	}
	
	if (!proc->vmm.table) {
		kvmm2_init(&proc->vmm);
	}
	
	KCCENTER(&ks->schedlock);
	
	th = (KTHREAD*)kmalloc(sizeof(KTHREAD));
	memset(th, 0, sizeof(KTHREAD));
	ll_add((void**)&proc->threads, th);
	
	/* so it does not get run yet */
	th->flags = KTHREAD_SLEEPING;
	
	KCCEXIT(&ks->schedlock);
	
	th->pc = ehdr->e_entry;
	th->cpsr = 0x60000000 | ARM4_MODE_USER;
	/* set stack */
	th->sp = 0x90000800;
	/* map timers for debugging */
	/*
		integrator-cp	0x13000000
		realview		0x10011000
	*/
	kvmm2_mapsingle(&proc->vmm, 0xb0000000, 0x10011000, TLB_C_AP_FULLACCESS);
	/* map serial output for debugging */
	/* 
		integrator-cp 	0x16000000
		realview		0x10009000
	*/
	kvmm2_mapsingle(&proc->vmm, 0xa0000000, 0x10009000, TLB_C_AP_FULLACCESS);
	/* map stack page (4K) */
	kvmm2_allocregionat(&proc->vmm, 1, 0x90000000, TLB_C_AP_FULLACCESS);
	
	kprintf("HEREA\n");
	
	/* --- ALLOC RING BUFFER INTO KERNEL SERVER THREAD PROCESS FIRST --- */
	kvmm2_allocregion(&ks->kservproc->vmm, 1, KMEMSIZE, 0, TLB_C_AP_PRIVACCESS, &out);

	kprintf("HEREB\n");
	
	kvmm2_getphy(&ks->vmm, (uintptr)ks->kservproc->vmm.table, &page);
	oldpage = arm4_tlbget1();
	arm4_tlbset1(page);
	asm("mcr p15, #0, r0, c8, c7, #0");
	
	er_init(&th->krx, (void*)out, KVIRPAGESIZE >> 1, 16 * 4, &katomic_lockspin_wfe8nr);
	er_init(&th->ktx, (void*)(out + (KVIRPAGESIZE >> 1)), KVIRPAGESIZE >> 1, 16 * 4, 0);
	
	arm4_tlbset1(oldpage);
	asm("mcr p15, #0, r0, c8, c7, #0");
	
	/* --- MAP RING BUFFER INTO USERSPACE OF THREAD PROCESS */
	/* find unused appropriate sized region in thread's address space */
	kvmm2_findregion(&proc->vmm, 1, KMEMSIZE, 0, 0, &out);
	//kprintf("VIRTPAGE:%x\n", out);
	/* map that region to the same physical pages used in kernel server address space */
	kvmm2_getphy(&ks->kservproc->vmm, (uintptr)th->krx.er, &out2);
	//kprintf("PHYPAGE:%x\n", out2);
	kvmm2_mapsingle(&proc->vmm, out, out2, TLB_C_AP_FULLACCESS);
	/* set userspace pointers for thread to appropriate values */
	th->urx = (RB*)out;
	th->utx = (RB*)(out + (KVIRPAGESIZE >> 1));;
	th->proc = proc;
	/* initialize signal container */
	mla_init(&th->signals, 20);
	
	/* map address space so we can work directly with it */
	kvmm2_getphy(&ks->vmm, (uintptr)proc->vmm.table, &page);
	oldpage = arm4_tlbget1();
	arm4_tlbset1(page);
	/* flush TLB */
	asm("mcr p15, #0, r0, c8, c7, #0");

	
	/* pass the arguments */
	th->r0 = (uint32)th->urx;
	th->r1 = (uint32)th->utx;
	th->r2 = (KVIRPAGESIZE >> 1) - sizeof(RB);
	
	// e_shoff - section table offset
	// e_shentsize - size of each section entry
	// e_shnum - count of entries in table
	for (x = 0; x < ehdr->e_shnum; ++x) {
		shdr = (ELF32_SHDR*)(addr + ehdr->e_shoff + x * ehdr->e_shentsize);
		if (shdr->sh_addr != 0) {
			/* load this into memory */
			// sh_offset - byte offset in module
			// sh_size - size of section in module 
			// sh_addr - address to load at
			
			/*
				The KVMM2_ALLOCREGION_NOFIND basically says to allocate a region
				starting at the specified address, BUT ignore any pages that have
				already been allocated. This helps when sections overlap on pages
				and would cause it to fail because the memory is already mapped.
			*/
			kprintf("elf-load    virtual:%x size:%x\n", shdr->sh_addr, shdr->sh_size);
			
			/*
				allocregionat will drop the lower 12 bits, and doing so rndup might result
				in the value of 1, but if sh_addr+sh_size crossed a page boundary then we
				need to allocate one more page to cover that
			*/
			if (((shdr->sh_addr + shdr->sh_size) & ~0xFFF) != (shdr->sh_addr & ~0xFFF)) {
				y = kvmm2_rndup(shdr->sh_size) + 1;
			} else {
				y = kvmm2_rndup(shdr->sh_size);
			}
			
			kvmm2_allocregionat(	&proc->vmm, 
									y, 
									shdr->sh_addr, 
									TLB_C_AP_FULLACCESS | KVMM2_ALLOCREGION_NOFIND
			);
			
			fb = (uint8*)(addr + shdr->sh_offset);
			/* copy */
			//kprintf("    elf copying\n");
			for (y = 0; y < shdr->sh_size; ++y) {
				((uint8*)shdr->sh_addr)[y] = fb[y];
				//kprintf("     y:%x\n", y + shdr->sh_addr);
			}
			//kprintf("    done\n");
			//for (y = 0; y < 0x6ffffff; ++y);
		}
	}
	
	/* restore previous address space */
	arm4_tlbset1(oldpage);
	/* flush TLB */
	asm("mcr p15, #0, r0, c8, c7, #0");
	kprintf("     elf-load: DONE\n");
	/* allow it to be run */
	th->flags = 0;
	return th;
}

/*
	@sdescription:			Used by kernel thread to allocate kernel heap memory.
	@ldescription:			This happens because the kernel heap memory interface
							is protected by per-CPU locks instead of per-thread
							locks. 
*/
void *tkmalloc(uint32 size) {
	uintptr		result;
	kprintf("beginning call\n");
	asm volatile(
		"push {r0}\n"
		"mov r0, %[in]\n"
		"swi %[code]\n"
		"mov %[out], r0\n"
		"pop {r0}\n"
		: [out]"=r" (result) : [in]"r" (size), [code]"i" (KSWI_TKMALLOC));
	kprintf("tkmalloc:%x\n", result);
	return (void*)result;
}

/*
	@sdescription:			Used by a kernel thread to add a thread to a process.
*/
int tkaddthread(KPROCESS *proc, KTHREAD *th) {
	int			result;
	asm volatile(
		"push {r0, r1}\n"
		"mov r0, %[proc]\n"
		"mov r1, %[thread]\n"
		"swi %[code]\n"
		"mov %[out], r0\n"
		"pop {r0, r1}\n"
		: [out]"=r" (result) : [proc]"r" (proc), [thread]"r" (th), [code]"i" (KSWI_TKADDTHREAD));
	return result;
}

/*
	@sdescription:			A sleep for ticks function for kernel threads.
*/
int ksleep(uint32 timeout) {
	int			result;
	asm volatile (
				"push {r0}\n"
				"mov r0, %[in] \n"
				"swi #101 \n"
				"mov %[result], r0\n"
				"pop {r0}\n"
				: [result]"=r" (result) : [in]"r" (timeout));
	/* convert from ticks */
	return result;
}

/*
	@sdescription:		Produces a peusudo random number from a integer passed in.
*/
static uint32 rand(uint32 next)
{
    next = next * 1103515245 + 12345;
    return (uint32)(next / 65536) % 32768;
}

/*
	@group:				Kernel.KThread
	@sdescription:		Will verify that specified process and
						thread exists and pointers are valid.
	@param:>tarproc:	specified process
	@param:>tarth:		specified thread
	@return:			positive integer means success
*/
int kthread_threadverify(KPROCESS *tarproc, KTHREAD *tarth) {
	uint32			x, y;
	KPROCESS		*proc;
	KTHREAD			*th;
	KSTATE			*ks;
	
	ks = GETKS();
	
	kprintf("[kthread] threadverify iterating process/thread chains\n");
	y = 1;
	while (y) {
		/* grab lock on deallocation array and free threads */		
		x = 0;
		y = 0;
		for (proc = ks->procs; proc; proc = proc->next) {
			if (!proc || (proc->flags & KPROCESS_DEAD)) {
				y = 1;
				kprintf("[kthread] iteration RESET\n");
				break;
			}
			
			if (tarproc != proc) {
				continue;
			}
			
			y = 0;
			for (th = proc->threads; th; th = th->next) {
				if (!th || (th->flags & KTHREAD_DEAD)) {
					y = 1;
					kprintf("[kthread] iteration RESET\n");
					break;
				}
				if (tarth == th) {
					return 1;
				}
			}
			if (y) {
				break;
			}
		}
	}
	/* invalid */
	return 0;
}

/*
	@group:				Kernel.KThread
	@sdescription:		Provides SWI wrapped for mapping
						shared memory between two processes.
	@param:acceptor:	process that is accepting this memory
						share operation
	@param:requestor:	process that requested this memory
						share operation
	@param:addr:		address of page aligned data that is
						already mapped into the requestor
						proces
	@param:pcnt:		count of contigious pages
	@param:>out:		holds the address of the memory mapped
						into the acceptor process
	@return:			positive integer is successful
*/
int __attribute__((naked)) tksharememory(
				KPROCESS *acceptor, KPROCESS *requestor, 
				uintptr addr, uintptr pcnt, uintptr *out) {
	asm volatile (
		"swi %[code]\n"
		"ldr ip, [sp]\n"
		"str r1, [ip]\n"
		"bx lr\n"
		: [out]"=r" (out)
		: [acceptor]"r" (acceptor),
		  [requestor]"r" (requestor),
		  [addr]"r" (addr),
		  [pcnt]"r" (pcnt),
		  [code]"i" (KSWI_TKSHAREMEM)
	);
}

/*
	@group:			Kernel.KThread
	@sdescription:	The kernel server thread. Handles everything
					that the SWI interface does not handle.
	@ldescription:	Can handle multiple instances by different
					threads allowing each CPU for this specific
					kernel to help share the load of requests.
	@param:myth:	pointer to thread executing this procedure
*/
void kthread(KTHREAD *myth) {
	uintptr			x, y, z;
	KPROCESS		*proc;
	KTHREAD			*th, *_th;
	KSTATE			*ks;
	uint32			pkt[32];
	uint32			sz;
	uintptr			out;
	int				ret;
	uint32			n;
	uint32 volatile	*t0mmio;
	uint32			st;
	uint32			tt, bytes;
	uint32			cycle;
	void			*ptr;
	MWSRGLA_BLOCK	*mwsrglab;
	uint32			mwsrglandx;

	
	KTHREAD			**toc, **_toc;
	uint32			tocmax;
	
	ks = GETKS();
	
	/* initial array allocation */
	tocmax = 10;
	toc = (KTHREAD**)kmalloc(sizeof(KTHREAD*) * tocmax);
	memset(toc, 0, sizeof(KTHREAD*) * tocmax);
	
	for (;;) {
		kprintf("[kthread] deallocing on MWSR\n");
		/* grab deallocated objects and free them */
		/* TODO: FOR MULTIPLE KTHREADS PUT INTER-THREAD LOCK-A AQUIRE HERE */
		while(mwsr_getone(&ks->dealloc, (uintptr*)&ptr)) {
			kfree(ptr);
			kprintf("[kthread] dealloc %x\n", ptr);
		}
		/* TODO: FOR MULTIPLE KTHREADS PUT INTER-THREAD LOCK-A RELEASE HERE */
		
		/* check ring buffers only for threads that have signaled us */		
		mwsrglab = 0;
		mwsrglandx = 0;
		
		kprintf("[kthread] checking MWSRGLA\n");
		/* mwsrgla has a per-thread lock so it supports multiple threads */
		while (mwsrgla_get(&ks->ktsignal, &mwsrglab, &mwsrglandx, (uintptr*)&th)) {
			/* remove DOS prevention flag */
			th->flags &= ~KTHREAD_WAKINGUPKTHREAD;
			
			kprintf("[testuelf] [kthread] reading RB for thread:%x[%s]\n", th, th->dbgname);
			for (sz = sizeof(pkt); er_read_nbio(&th->ktx, &pkt[0], &sz); sz = sizeof(pkt)) {
				kprintf("[kthread] got pkt\n");
				kprintf("[testuelf] [kthread] got pkt %x\n", pkt[0]);
				/* check packet type */
				switch (pkt[0]) {
					case KMSG_SENDMESSAGE:
						kprintf("[kthread] send message to process:%x thread:%x\n", pkt[2], pkt[3]);
						/* send message to another process:thread */
						if (kthread_threadverify((KPROCESS*)pkt[2], (KTHREAD*)pkt[3])) {
							// pkt[0] - type
							// pkt[1] - request-id
							// pkt[2] - process id
							// pkt[3] - thread id
							// pkt[4-31] - payload
						
							/* add message to ring buffer */
							_th = (KTHREAD*)pkt[3];
							pkt[0] = KMSG_THREADMESSAGE;
							sz = sizeof(pkt);
							/* put sender's process:thread id in */
							pkt[2] = (uint32)th->proc;
							pkt[3] = (uint32)th;
							er_write_nbio(&_th->krx, &pkt[0], sz);
							th->flags |= KTHREAD_WAKEUP;
						} else {
							/* send message back for error */
							kprintf("[kthread] send failed - no proc/thread match\n");
							pkt[0] = KMSG_SENDFAILED;
							sz = sizeof(pkt);
							er_write_nbio(&th->krx, &pkt[0], sz);
							th->flags |= KTHREAD_WAKEUP;
						}
						break;
					case KMSG_CREATETHREAD:
						/* fill default stuff */
						kprintf("[kthread] create thread message RID:%x pc:%x\n", pkt[1], pkt[2]);
						_th = (KTHREAD*)tkmalloc(sizeof(KTHREAD));
						_th->r0 = pkt[4];
						_th->r1 = pkt[5];
						_th->r2 = pkt[6];
						_th->r3 = pkt[7];
						_th->r4 = pkt[8];
						_th->r5 = pkt[9];
						_th->r6 = pkt[10];
						_th->r7 = pkt[11];
						_th->r8 = pkt[12];
						_th->r9 = pkt[13];
						_th->r10 = pkt[14];
						/* */
						_th->r11 = pkt[3];
						_th->r12 = pkt[2];
						_th->sp = _th->r11;
						_th->pc = _th->r12;
						/* */
						_th->cpsr = 0x60000000 | ARM4_MODE_USER;
						_th->flags = 0;
						_th->dbgname = "CREATEDTHREAD";
						_th->proc = th->proc;
						tkaddthread(th->proc, _th);
						break;
					case KMSG_REGSERVICE:
						// pkt[0] - type
						// pkt[1] - RID
						// pkt[2] - service id
						// pkt[3] - process id
						// pkt[4] - thread id
						if (pkt[3] == 0) {
							pkt[3] = (uintptr)th->proc;
							pkt[4] = (uintptr)th;
						}
						
						/* register service */
						if (!(th->proc->flags & KPROCESS_SYSTEM)) {
							pkt[0] = KMSG_REGSERVICEFAILD;
							er_write_nbio(&th->krx, &pkt[0], sz);
						} else {
							kprintf("[kthread] service registered for %x as %x:%x\n", pkt[2] & 3, pkt[3], pkt[4]);
							pkt[0] = KMSG_REGSERVICEOK;
							ks->ssr_proc[pkt[2] & 3] = pkt[3];
							ks->ssr_th[pkt[2] & 3] = pkt[4];
							er_write_nbio(&th->krx, &pkt[0], sz);							
						}
						th->flags |= KTHREAD_WAKEUP;
						break;
					case KMSG_ENUMSERVICE:
						// request:
						// 	pkt[0] - type
						// 	pkt[1] - rid
						// 	pkt[2] - service id
						// replay:
						// 	pkt[0] - type
						//  pkt[1] - rid
						//  pkt[2] - service id
						//  pkt[3] - process id
						//  pkt[4] - thread id
						/* enumerate services */
						kprintf("[kthread] enum service request\n");
						pkt[0] = KMSG_ENUMSERVICEREPLY;
						pkt[3] = ks->ssr_proc[pkt[2] & 3];
						pkt[4] = ks->ssr_th[pkt[2] & 3];
						kprintf("[kthread] returning %x:%x\n", pkt[3], pkt[4]);
						er_write_nbio(&th->krx, &pkt[0], 4 * 5);
						th->flags |= KTHREAD_WAKEUP;
						kprintf("th->flags:%x\n",  th->flags);
						break;
					case KMSG_REQSHARED:
						// pkt[0] - type
						// pkt[1] - RID
						// pkt[2] - memory offset
						// pkt[3] - page count
						// pkt[4] - target process
						// pkt[5] - target thread
						// ------ application specific (not enforced) -----
						// pkt[6] - signal to be used for requestor
						// pkt[7] - protocol expected
						// pkt[8] - TX buffer size expected
						// pkt[9] - RX buffer size expected
						
						/* do verification (to prevent expoiting) */
						if (kthread_threadverify((KPROCESS*)pkt[4], (KTHREAD*)pkt[5])) {
							kprintf("[kthread] verify good sending requent and reply\n");
							th->shreq_rid = pkt[1];
							th->shreq_memoff = pkt[2];
							th->shreq_pcnt = pkt[3];
							/* reply we are good */
							pkt[0] = KMSG_REQSHAREDOK;
							er_write_nbio(&th->krx, &pkt[0], sz);
							/* send request to target */
							_th = (KTHREAD*)pkt[5];
							pkt[0] = KMSG_REQSHARED;
							pkt[4] = (uintptr)th->proc;
							pkt[5] = (uintptr)th;
							er_write_nbio(&_th->krx, &pkt[0], sz);
							_th->flags |= KTHREAD_WAKEUP;
						} else {
							kprintf("[kthread] req shared failed; sending reply\n");
							pkt[0] = KMSG_REQSHAREDFAIL;
							er_write_nbio(&th->krx, &pkt[0], sz);
						}
						th->flags |= KTHREAD_WAKEUP;
						break;
					case KMSG_ACPSHARED:
						// pkt[0] - type
						// pkt[1] - rid
						// pkt[2] - address
						// pkt[3] - page count
						// pkt[4] - target process
						// pkt[5] - target thread
						// pkt[6] - target rid
						// pkt[6] - signal to be used for acceptor (requestor asserts this signal)
						// pkt[.] - extra data
						/* double check process:thread exist and active request is outstanding */
						if (kthread_threadverify((KPROCESS*)pkt[4], (KTHREAD*)pkt[5])) {
							_th = (KTHREAD*)pkt[5];
							printf("_th->shreq_memoff:%x(%x) _th->shreq_pcnt:%x(%x) _th->shreq_rid:%x(%x)\n",
								_th->shreq_memoff, pkt[2],
								_th->shreq_pcnt, pkt[3],
								_th->shreq_rid, pkt[6]
							);
							if ( /* make sure it all matches up */
								pkt[2] && pkt[3] && 
								pkt[2] == _th->shreq_memoff && 
								pkt[3] == _th->shreq_pcnt &&
								pkt[6] == _th->shreq_rid
								) {
								printf("[kthread] sharing memory..\n");
								tksharememory(th->proc, (KPROCESS*)pkt[4], pkt[2], pkt[3], &out);
								printf("[kthread]      ... done\n");
								
								/* let them know it succeded and who for */
								pkt[0] = KMSG_ACPSHAREDOK;
								x = pkt[4];
								y = pkt[5];
								z = pkt[1];
								pkt[4] = (uintptr)th->proc;
								pkt[5] = (uintptr)th;
								pkt[1] = _th->shreq_rid;
								er_write_nbio(&_th->krx, &pkt[0], sz);
								_th->flags |= KTHREAD_WAKEUP;
								
								/* let them know it succeded and let other end know */
								pkt[1] = z;
								pkt[2] = out;		/* tell acceptor their memory address */
								pkt[4] = x;
								pkt[5] = y;
								er_write_nbio(&th->krx, &pkt[0], sz);
								printf("[kthread] memory shared; sent notifications\n");
							} else {
								printf("[kthread] ACPSHARED fail due to request not standing.\n");
								/* it failed because request was not there */
								pkt[0] = KMSG_ACPSHAREDFAIL;
								er_write_nbio(&th->krx, &pkt[0], sz);
							}
							th->flags |= KTHREAD_WAKEUP;
						} else {
							printf("[kthread] ACPSHARED fail due to bad process:thread address\n");
							/* let them know it failed */
							pkt[0] = KMSG_ACPSHAREDFAIL;
							er_write_nbio(&th->krx, &pkt[0], sz);
						}
						th->flags |= KTHREAD_WAKEUP;
						break;
					default:
						kprintf("[kthread] unknown message type:%x size:%x\n", pkt[0], sz);
						break;
				}
			}
			kprintf("[kthread] done reading RB\n");
		}
		kprintf("[kthread] sleeping\n");
		ksleep(ks->tpers * 100);
	}
}

/*
	@sdescription:			A thread the CPU runs when it has no other task to run.
	@ldescription:			A thread the CPU runs when it has no other tasks to run.
							This is likely not a very efficent way to do this. I will
							likey make the scheduler issue this instruction directly
							in the code and remove this function later.
*/
void kidle() {
	for (;;) {
		/* everything is sleeping, then sleep the CPU */
		asm("wfe");
	}
}

/*
	@sdescription:		Does common work not specific to any board or platform. This
						is called by the board module once it has configured the
						basic required systems.
*/
void start() {
	uint32		*t0mmio;
	uint32		*picmmio;
	uint8		*a9picmmio;
	KSTATE		*ks;
	int			x;
	uint32		lock;
	uint32		*utlb, *ktlb;
	uint32		*a, *b;
	uint8		*bm;
	uint32		*tlbsub;
	uintptr		page;
	uintptr		__vmm;
	KVMMTABLE	test;
	uintptr		eoiwmods;
	KATTMOD		*m;
	KPROCESS	*process;
	KTHREAD		*th;
	
	uint32		cpuid;
	uint32		*scu;
	
	asm("mrc p15, 0, %[cpuid], c0, c0, 5" : [cpuid]"=r" (cpuid));
	kprintf("cpuid:%x\n", cpuid);

	/* system specific initialization (currently included with board module) */
	systemPreInitialization();

	kprintf("@@@\n");
	
	ks = GETKS();
	
	kserdbg_putc('Y');
	
	kserdbg_putc('D');
	
	/*
		This should be provided by an partialy external module. It
		can be different for different boards and is choosen during
		linking of the kernel image.
		
		It also enables paging!!
	*/
	kboardPrePagingInit();	
	
	/*
		PAGING SHOULD BE ENABLED IF USED
	*/
		
	kserdbg_putc('U');
	
	/* testing something GCC specific (built-in atomic locking) */
	//while (__sync_val_compare_and_swap(&lock, 0, 1));
	
	kserdbg_putc('G');
	
	/* test kstack (make it cause problems if broken) */
	for (x = 0; x < 4096; ++x) {
		kstack_push(&ks->tstack, 0);
	}
	for (x = 0; x < 4096; ++x) {
		kstack_pop(&ks->tstack, &lock);
	}
		
	kserdbg_putc('Z');
	
	process = (KPROCESS*)kmalloc(sizeof(KPROCESS));
	memset(process, 0, sizeof(KPROCESS));
	kvmm2_init(&process->vmm);
	ll_add((void**)&ks->procs, process);
	
	th = (KTHREAD*)kmalloc(sizeof(KTHREAD));
	memset(th, 0, sizeof(KTHREAD));
	ll_add((void**)&process->threads, th);
	th->pc = (uintptr)&kthread;
	th->flags = 0;
	th->dbgname = "KTHREAD";
	th->cpsr = 0x60000000 | ARM4_MODE_SYS;
	/* set stack */
	th->sp = (uintptr)kmalloc(1024 * 2) + 1024 * 2;
	th->sp = th->sp & ~15;
	kprintf("kthread initial-sp:%x\n", th->sp);
	((uint32*)th->sp)[0] = 0x12345678;
	th->r0 = (uint32)th;
	th->proc = process;
	ks->kservthread = th;
	ks->kservproc = process;
	mla_init(&th->signals, 20);
	
	th = (KTHREAD*)kmalloc(sizeof(KTHREAD));
	memset(th, 0, sizeof(KTHREAD));
	ll_add((void**)&process->threads, th);
	th->pc = (uintptr)&kidle;
	th->flags = KTHREAD_KIDLE;
	th->dbgname = "KIDLE";
	th->cpsr = 0x60000000 | ARM4_MODE_SYS;
	/* set stack (dont need anything big for idle thread at the moment) */
	th->sp = (uintptr)kmalloc(128) + 128 - 8;
	th->r0 = (uint32)th;
	th->proc = process;
	ks->idleth = th;
	ks->idleproc = process;
	
	ks->tmpsum = 0;
	ks->tmpcnt = 0;
	
	/*
		this allows deallocated threads and process structures
		to be kept along for the kthread to use, and allows it
		to free them as needed
	*/
	mwsr_init(&ks->dealloc, 10);
	/*
		this is used to hold active signals for kthread to service
		the ring buffer of specified thread
	*/
	mwsrgla_init(&ks->ktsignal, 10);
		
	#define KMODTYPE_ELFUSER			1
	/*
		create a task for any attached modules of the correct type
	*/

	x = 0;
	kprintf("looking at attached modules\n");
	for (m = kPkgGetFirstMod(); m; m = kPkgGetNextMod(m)) {
		kprintf("looking at module\n");
		if (m->type == KMODTYPE_ELFUSER) {
			/* create new process */
			process = (KPROCESS*)kmalloc(sizeof(KPROCESS));
			memset(process, 0, sizeof(KPROCESS));
			/* system processes (loaded with kernel) */
			process->flags |= KPROCESS_SYSTEM;
			ll_add((void**)&ks->procs, process);
			/* will create thread in process */
			th = kelfload(process, (uintptr)&m->slot[0], m->size);
			switch (x) {
				case 0:
					th->dbgname = "USERMODULE1";
					break;
				case 1:
					th->dbgname = "USERMODULE2";
					break;
				default:
					th->dbgname = "USERMODULEX";
					break;
			}
			++x;
		}
		kprintf("looking for NEXT module..\n");
	}
	
	//kprintf("....\n");
		
	kprintf("kboardPostPagingInit()\n");	
	kboardPostPagingInit();
	
	kserdbg_putc('Q');
	kserdbg_putc('\n');
	/* 
		infinite loop 
		
		if we wanted we could create a thread for this current executing context, and call it
		the kernel thread, but i just let it die off (need to reclaim stack later though)
	*/
	for(;;);
}