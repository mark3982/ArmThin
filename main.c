/*
	http://wiki.osdev.org/User:Pancakes/BitmapHeapImplementation
	
	I just placed all the structs and prototypes into a seperate header.
	
	You need to create this header, and place the implementation into another
	source file named for example kheap_bm.c. Then compile and link that with
	this source file.
*/
#include "stdtypes.h"
#include "main.h"
#include "kheap.h"
#include "vmm.h"
#include "kmod.h"
#include "atomic.h"

extern uint8 _BOI;
extern uint8 _EOI;

void start(void);

/*
    This could be non-standard behavior, but as long as this resides at the top of this source and it is the
    first file used in the linking process (according to alphanumerical ordering) this code will start at the
    beginning of the .text section.
*/
void __attribute__((naked)) __attribute__((section(".boot"))) entry() {
	/* branch to board code */
	asm volatile ("b boardEntry");
}

/* small chunk memory alloc/free */
void kfree(void *ptr) {
	KSTATE			*ks;
	
	ks = GETKS();

	k_heapBMFree(&ks->hchk, ptr);
}

void* kmalloc(uint32 size) {
	void			*ptr;
	KSTATE			*ks;
	uint32			_size;	
	
	ks = GETKS();
	
	/* attempt first allocation try (will fail on very first) */
	ptr = k_heapBMAlloc(&ks->hchk, size);
	
	/* try adding more memory if failed */
	if (!ptr) {
		if (size < KCHKMINBLOCKSZ / 2) {
			/* we need to allocate blocks at least this size */
			_size = KCHKMINBLOCKSZ;
		} else {
			/* this is bigger than KCHKMINBLOCKSZ, so lets double it to be safe */
			/* round up allocation to use all the blocks taken */
			_size = size * 2;
			_size = (_size / KPHYPAGESIZE) * KPHYPAGESIZE < _size ? _size / KPHYPAGESIZE + 1 : _size / KPHYPAGESIZE;
			_size = _size * KPHYPAGESIZE;
		}
		ptr = k_heapBMAlloc(&ks->hphy, _size);
		if (!ptr) {
			/* no more physical pages */
			return 0;
		}
		/* TODO: need to allocate virtual memory!! */
		kserdbg_puts("WARNING: UNIMPLEMENTED/UNFINISHED ALLOC FOR KMALLOC NEW BLOCK\n");
		for(;;);
		/* try allocation once more, should succeed */
		k_heapBMAddBlock(&ks->hchk, (uintptr)ptr, _size, KCHKHEAPBSIZE);
		ptr = k_heapBMAlloc(&ks->hchk, size);
	}
	
	return ptr;
}
		
uint32 arm4_cpsrget()
{
    uint32      r;
    
    asm("mrs %[ps], cpsr" : [ps]"=r" (r));
    return r;
}

uint32 arm4_spsrget()
{
    uint32      r;
    
    asm("mrs %[ps], spsr" : [ps]"=r" (r));
    return r;
}

void arm4_cpsrset(uint32 r)
{
    asm("msr cpsr, %[ps]" : : [ps]"r" (r));
}
	
void arm4_xrqenable_fiq()
{
    arm4_cpsrset(arm4_cpsrget() & ~(1 << 6));
}

void arm4_xrqenable_irq()
{
    arm4_cpsrset(arm4_cpsrget() & ~(1 << 7));
}

void arm4_tlbset0(uint32 base) {
	asm("mcr p15, 0, %[tlb], c2, c0, 0" : : [tlb]"r" (base));
}

void arm4_tlbset1(uint32 base) {
	asm("mcr p15, 0, %[tlb], c2, c0, 1" : : [tlb]"r" (base));
}

uint32 arm4_tlbget1() {
	uint32			base;
	asm("mrc p15, 0, %[tlb], c2, c0, 1" : [tlb]"=r" (base));
	return base;
}

void arm4_tlbsetmode(uint32 val) {
	asm("mcr p15, 0, %[tlb], c2, c0, 2" : : [tlb]"r" (val));
}

void arm4_tlbsetdom(uint32 val) {
	asm("mcr p15, 0, %[val], c3, c0, 0" : : [val]"r" (val));
}

uint32 arm4_tlbgetctrl() {
	uint32			ctrl;
	asm("mrc p15, 0, %[ctrl], c1, c0, 0" : [ctrl]"=r" (ctrl));
	return ctrl;
}

void arm4_tlbsetctrl(uint32 ctrl) {
	asm("mcr p15, 0, %[ctrl], c1, c0, 0" : : [ctrl]"r" (ctrl));
}

/* physical page memory alloc/free */
void* kpalloc(uint32 size) {
	KSTATE			*ks;
	
	ks = GETKS();
	return k_heapBMAlloc(&ks->hphy, size);
}

void kpfree(void *ptr) {
	KSTATE			*ks;
	
	ks = GETKS();
	k_heapBMFree(&ks->hphy, ptr);
}

typedef struct _LL {
	struct _LL			*next;
	struct _LL			*prev;
} LL;

void ll_add(void **p, void *i) {
	LL		*_i;
	
	_i = (LL*)i;
	
	_i->next = *p;
	if (*p) {
		_i->prev = ((LL*)(*p))->prev;
	} else {
		_i->prev = 0;
	}
	
	if (p) {
		*p = _i;
	}
}

void ll_rem(void **p, void *i) {
	LL			*_i;
	
	_i = (LL*)i;

	if (_i->prev) {
		_i->prev->next = _i->next;
	}
	
	if (_i->next) {
		_i->next->prev = _i->prev;
	}
	
	if (p) {
		if (*p == i) {
			if (_i->prev) {
				*p = _i->prev;
			} else {
				*p = _i->next;
			}
		}
	}
}

void kdumpthreadinfo(KTHREAD *th) {
	kprintf("r0:%x\tr1:%x\tr2:%x\tr3:%x\n", th->r0, th->r1, th->r2, th->r3);
	kprintf("r4:%x\tr5:%x\tr6:%x\tr7:%x\n", th->r4, th->r5, th->r6, th->r7);
	kprintf("r8:%x\tr9:%x\tr10:%x\tr11:%x\n", th->r8, th->r9, th->r10, th->r11);
	kprintf("r12:%x\tsp:%x\tlr:%x\tcpsr:%x\n", th->r12, th->sp, th->lr, th->cpsr);
}

void ksched() {
	KSTATE			*ks;
	KTHREAD			*kt;
	uint32			__lr, __sp, __spsr;
	uintptr			page;
	uint32			tmp0, tmp1;
	
	ks = GETKS();

	/*
		1. store register on stack in thread struct
		2. access hidden registers and store in thread struct
	*/
	kt = ks->cthread;
	kt->pc = ((uint32*)KSTACKEXC)[-1];
	kt->r12 = ((uint32*)KSTACKEXC)[-2];
	kt->r11 = ((uint32*)KSTACKEXC)[-3];
	kt->r10 = ((uint32*)KSTACKEXC)[-4];
	kt->r9 = ((uint32*)KSTACKEXC)[-5];
	kt->r8 = ((uint32*)KSTACKEXC)[-6];
	kt->r7 = ((uint32*)KSTACKEXC)[-7];
	kt->r6 = ((uint32*)KSTACKEXC)[-8];
	kt->r5 = ((uint32*)KSTACKEXC)[-9];
	kt->r4 = ((uint32*)KSTACKEXC)[-10];
	kt->r3 = ((uint32*)KSTACKEXC)[-11];
	kt->r2 = ((uint32*)KSTACKEXC)[-12];
	kt->r1 = ((uint32*)KSTACKEXC)[-13];
	kt->r0 = ((uint32*)KSTACKEXC)[-14];
	kt->cpsr = ((uint32*)KSTACKEXC)[-15];
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
	
	while (1) {
		/* get next thread */
		{
			/* get next thread */
			if (ks->cthread && ks->cthread->next) {
				ks->cthread = ks->cthread->next;
			} else {
				if (ks->cproc->next) {
					ks->cproc = ks->cproc->next;
				} else {
					ks->cproc = ks->procs;
				}
				ks->cthread = ks->cproc->threads;
			}
			/* keep going until we have a valid thread */
		} while (!ks->cthread);
			
		/* if current thread is sleeping and current thread equals last thread */
		if ((ks->cthread->flags & KTHREAD_SLEEPING) && (ks->cthread == kt)) {
			PANIC("all-threads-sleeping");
		}
		
		/* only wakeup if it is sleeping */
		if (ks->cthread->flags & KTHREAD_SLEEPING) {
			if (ks->cthread->timeout > 0 && ks->ctime > ks->cthread->timeout) {
				//kprintf("WOKE UP (timeout) %x %x\n", ks->cthread, ks->cthread->timeout);
				/* wake up thread if passed timeout */
				ks->cthread->flags &= ~KTHREAD_SLEEPING;
				ks->cthread->r0 = 0;
				break;
			}
			
			/* wakeup thread is set to be woken up */
			if (ks->cthread->flags & KTHREAD_WAKEUP) {
				//kprintf("WOKE UP (signal) %x\n", ks->cthread);
				ks->cthread->flags &= ~(KTHREAD_WAKEUP | KTHREAD_SLEEPING);
				ks->cthread->r0 = ks->ctime - ks->cthread->timeout;
				break;
			}
		} else {
			/* run this thread */
			break;
		}
		/* thread is sleeping or not able to run */
	}
	
	/* hopefully we got something or the system should deadlock */
	kt = ks->cthread;
	/*
		load registers
	*/
	((uint32*)KSTACKEXC)[-1] = kt->pc;
	((uint32*)KSTACKEXC)[-2] = kt->r12;
	((uint32*)KSTACKEXC)[-3] = kt->r11;
	((uint32*)KSTACKEXC)[-4] = kt->r10;
	((uint32*)KSTACKEXC)[-5] = kt->r9;
	((uint32*)KSTACKEXC)[-6] = kt->r8;
	((uint32*)KSTACKEXC)[-7] = kt->r7;
	((uint32*)KSTACKEXC)[-8] = kt->r6;
	((uint32*)KSTACKEXC)[-9] = kt->r5;
	((uint32*)KSTACKEXC)[-10] = kt->r4;
	((uint32*)KSTACKEXC)[-11] = kt->r3;
	((uint32*)KSTACKEXC)[-12] = kt->r2;
	((uint32*)KSTACKEXC)[-13] = kt->r1;
	((uint32*)KSTACKEXC)[-14] = kt->r0;
	((uint32*)KSTACKEXC)[-15] = kt->cpsr;
	/* switch into system mode restore hidden registers then switch back */
	asm volatile ("mrs %[tmp0], cpsr \n\
		 mov %[tmp1], %[tmp0] \n\
		 bic %[tmp0], %[tmp0], #0x1f \n\
		 orr %[tmp0], %[tmp0], #0x1f \n\
		 msr cpsr, %[tmp0] \n\
		 mov sp, %[sp] \n\
		 mov lr, %[lr] \n\
		 msr cpsr, %[tmp1] \n\
		 " : [tmp0]"+r" (tmp0), [tmp1]"+r" (tmp1) : [sp]"r" (kt->sp), [lr]"r" (kt->lr));
	/* set TLB table for user space (it can be zero for kernel) */
	kvmm2_getphy(&ks->vmm, (uintptr)ks->cproc->vmm.table, &page);
	arm4_tlbset1(page);
	/* 
		Invalidate all unlocked entries...
		
		..according to the manual there may be a better way to invalidate,
		only some entries per process. But, for now this should work.
		
		If you do not do this then the TLB does not flush and old entries
		from the previous process will still be in the TLB cache.
	*/
	asm("mcr p15, #0, r0, c8, c7, #0");
	
	kprintf("SWITCH-TO thread:%x cpsr:%x fp:%x sp:%x pc:%x dbgname:%s\n", kt, kt->cpsr, kt->r11, kt->sp, kt->pc, kt->dbgname);
	
	uint32			*p;
	
	if (!kvmm2_getphy(&ks->cproc->vmm, 0x90000000, (uintptr*)&p)) {
		//kprintf("NO STACK EXISTS??\n");
	} else {
		//kprintf("writing to stack..%x\n", kt->sp);
		//((uint32*)kt->sp)[-1] = 0xbb;
	}
	
	if (kvmm2_getphy(&ks->cproc->vmm, 0x80000000, (uintptr*)&p)) {
		uint32			x;
		
		//kprintf("CODE PAGE :%x\n", p);
		
		p = (uint32*)(0x80000000);

		//((uint32*)KSTACKEXC)[-1] = 0x80000800;
	
		//for (x = 0; x < 1024; ++x) {
		//	p[x] = 0xeafffffe;
		//}
	
	} else {
		//kprintf("CODE PAGE????\n");
	}
}

static void kprocfree__walkercb(uintptr v, uintptr p) {
	KSTATE					*ks;
	uint32					r0;
	
	ks = GETKS();

	/* just unmap it to zero the entry for safety */
	kvmm2_unmap(&ks->cproc->vmm, v, 0);
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

int kprocfree(KPROCESS *proc) {
	kprintf("kprocfree...\n");
	/* walk entries and free them */
	kvmm2_walkentries(&proc->vmm, &kprocfree__walkercb);
	
	return 1;
}

//__attribute__((optimize("O0")))
void __attribute__ ((noinline)) k_exphandler(uint32 lr, uint32 type) {
	uint32 volatile	*t0mmio;
	uint8 volatile	*picmmio;
	uint32			swi;
	KSTATE			*ks;
	int				x;
	KTHREAD			*kt;
	uintptr			out;
	uint32			r0, r1;
	KPROCESS		*proc;
	KTHREAD			*th, *_th;
	
	ks = GETKS();
		
	//kserdbg_putc('H');
	//kserdbg_putc('\n');
	
	if (type == ARM4_XRQ_IRQ) {
		if (kboardCheckAndClearTimerINT()) {			
			/* update current time */
			ks->ctime += kboardGetTimerTick();
			kprintf("IRQ: ctime:%x timer-tick:%x\n", ks->ctime, kboardGetTimerTick());
			ksched();
			kprintf("...EXIT\n");
			/* go back through normal interrupt return process */
			return;
		}
	}
	
	/*
		Get SWI argument (index).
	*/
	if (type == ARM4_XRQ_SWINT) {
		swi = ((uint32*)((uintptr)lr - 4))[0] & 0xffff;
		
		//kprintf("SWI thread:%x code:%x\n", ks->cthread, swi);
		
		//((uint32*)KSTACKEXC)[-14] = R0;
		//((uint32*)KSTACKEXC)[-13] = R1;
		
		switch (swi) {
			case KSWI_TERMPROCESS:
				/* unlink process and threads, but keep modify pointer so
				   scheduler can grab next thread */
				proc = ks->cproc;
				th = ks->cthread;
				_th = th->next;
				th->next = 0;
				/* should schedule a thread from the next process */
				ksched();
				/* restore pointer */
				th->next = _th;
				/* cycle through threads releasing resources */
				for (th = proc->threads; th; th = _th) {
					_th = th->next;
					/* release heap memory */
					kfree(th);
				}
				/* free process resources */
				if (!kprocfree(proc)) {
					PANIC("ERROR-FREEING-PROC-RESOURCES");
				}
				/* release heap memory */
				kfree(proc);
				break;
			case KSWI_TERMTHREAD:
				proc = ks->cproc;
				th = ks->cthread;
				/* should schedule a thread from the next process */
				ksched();
				/* now unlink and free thread resources */
				ll_rem((void**)&proc->threads, th);
				kfree(th);
				break;
			case KSWI_VALLOC:
				/* allocate range of pages and store result in R0 */
				r0 = ((uint32*)KSTACKEXC)[-14];
				kprintf("KSWI_VALLOC r0:%x\n", r0);
				kvmm2_allocregion(&ks->cproc->vmm, r0, KMEMSIZE, 0, TLB_C_AP_PRIVACCESS, &((uint32*)KSTACKEXC)[-14]);
				kprintf("            r0:%x\n", r0);
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
				r0 = ((uint32*)KSTACKEXC)[-14];
				r1 = ((uint32*)KSTACKEXC)[-13];
				kprintf("KSWI_VFREE r0:%x r1:%x\n", r0, r1);
				for (x = 0; x < r1; ++x) {
					/* unmap (dont free.. yet) */
					kvmm2_getphy(&ks->cproc->vmm, r0 + x * 0x1000, &out);
					kvmm2_unmap(&ks->cproc->vmm, r0 + x * 0x1000, 0);
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
			case KSWI_WAKEUP:
				/* wake up thread function */
				r0 = ((uint32*)KSTACKEXC)[-14];
				r1 = ((uint32*)KSTACKEXC)[-13];
				for (proc = ks->procs; proc; proc = proc->next) {
					if ((uint32)proc == r0) {
						for (th = proc->threads; th; th = th->next) {
							if ((uint32)th == r1) {
								/* wake up thread */
								th->flags |= KTHREAD_WAKEUP;
							}
						}
					}
				}
				break;
			case KSWI_GETTICKPERSECOND:
				((uint32*)KSTACKEXC)[-14] = ks->tpers;
				break;
			case KSWI_SLEEP:
				/* thread sleep function */
				r0 = ((uint32*)KSTACKEXC)[-14];
				
				//kprintf("SLEEPING thread:%x timeout:%x flags:%x\n", ks->cthread, r0, ks->cthread->flags);

				ks->cthread->flags |= KTHREAD_SLEEPING;
				if (r0 != 0) {
					kprintf("TEST %x\n", ks->ctime);
					kprintf("SLEEP total:%x r0:%x ks->ctime:%x\n", r0 + ks->ctime, r0, ks->ctime);
					ks->cthread->timeout = r0 + ks->ctime;
				} else {
					ks->cthread->timeout = 0;
				}

				ksched();
				break;
			case KSWI_YEILD:
				ksched();
				break;
			case KSWI_KERNELMSG:
				ks->kservthread->flags |= KTHREAD_WAKEUP;
				break;
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
		
		tmp = ks->cthread;
		
		kprintf("!EXCEPTION type:%x ks:%x cpu:%x ks->cthread:%x\n", type, ks, cpu, ks->cthread);
		kprintf("CPU:%x type:%x cproc:%x cthread:%x lr:%x dbgname:%s\n", cpu, type, ks->cproc, ks->cthread, lr, ks->cthread ? ks->cthread->dbgname : "$none$");

		asm("mrs r0, cpsr \n\
			 mov r1, r0 \n\
			 bic r0, r0, #0x1f \n\
			 orr r0, r0, #0x1f \n\
			 msr cpsr, r0 \n\
			 mov %[sp], sp \n\
			 mov %[lr], lr \n\
			 msr cpsr, r1 \n\
			 " : [sp]"=r" (__sp), [lr]"=r" (__lr));
		
		kprintf("sp:%x lr:%x\n", __sp, __lr);
		
		if (lr >= (uintptr)&_BOI && lr <= (uintptr)&_EOI) {
			PANIC("CRITICAL FAILURE: exception in kernel image\n");
		}
		
		for (;;);
		
		ll_rem((void**)&ks->cproc->threads, ks->cthread);
		ks->cthread = ks->cthread->next;
		
		ksched();
		
		kdumpthreadinfo(tmp);
	}
	
	return;
}
	
void __attribute__((naked)) k_exphandler_irq_entry() { KEXP_TOP3(ARM4_XRQ_IRQ); }
void __attribute__((naked)) k_exphandler_fiq_entry() { KEXP_TOP3(ARM4_XRQ_FIQ); }
void __attribute__((naked)) k_exphandler_reset_entry() { KEXP_TOP3(ARM4_XRQ_RESET); }
void __attribute__((naked)) k_exphandler_undef_entry() { KEXP_TOP3(ARM4_XRQ_UNDEF); }	
void __attribute__((naked)) k_exphandler_abrtp_entry() { KEXP_TOP3(ARM4_XRQ_ABRTP); }
void __attribute__((naked)) k_exphandler_abrtd_entry() { KEXP_TOP3(ARM4_XRQ_ABRTD); }
void __attribute__((naked)) k_exphandler_swi_entry() { KEXP_TOPSWI; k_exphandler(lr, ARM4_XRQ_SWINT); KEXP_BOTSWI; }

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

void memset(void *p, uint8 v, uintptr sz) {
	uint8 volatile		*_p;
	uintptr				x;
	
	_p = (uint8*)p;
	
	for (x = 0; x < sz; ++x) {
		_p[x] = v;
	}
	
	return;
}

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

	th = (KTHREAD*)kmalloc(sizeof(KTHREAD));
	memset(th, 0, sizeof(KTHREAD));
	ll_add((void**)&proc->threads, th);
	
	th->pc = ehdr->e_entry;
	th->flags = 0;
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
	
	/* --- ALLOC RING BUFFER INTO KERNEL SERVER THREAD PROCESS FIRST --- */
	kvmm2_allocregion(&ks->kservproc->vmm, 1, KMEMSIZE, 0, TLB_C_AP_PRIVACCESS, &out);
	kprintf("KSERV VIRTPAGE:%x\n", out);
	th->krx.rb = (RB*)out;
	th->krx.sz = (KVIRPAGESIZE >> 1) - sizeof(RB);			/* should be 2KB */
	th->ktx.rb = (RB*)(out + (KVIRPAGESIZE >> 1));
	th->ktx.sz = (KVIRPAGESIZE >> 1) - sizeof(RB);			/* should be 2KB */
	
	/* --- MAP RING BUFFER INTO USERSPACE OF THREAD PROCESS */
	/* find unused appropriate sized region in thread's address space */
	kvmm2_findregion(&proc->vmm, 1, KMEMSIZE, 0, 0, &out);
	kprintf("VIRTPAGE:%x\n", out);
	/* map that region to the same physical pages used in kernel server address space */
	kvmm2_getphy(&ks->kservproc->vmm, (uintptr)th->krx.rb, &out2);
	kprintf("PHYPAGE:%x\n", out2);
	kvmm2_mapsingle(&proc->vmm, out, out2, TLB_C_AP_FULLACCESS);
	/* set userspace pointers for thread to appropriate values */
	th->urx = (RB*)out;
	th->utx = (RB*)(out + (KVIRPAGESIZE >> 1));;
	
	/* map address space so we can work directly with it */
	kvmm2_getphy(&ks->vmm, (uintptr)proc->vmm.table, &page);
	oldpage = arm4_tlbget1();
	arm4_tlbset1(page);
	/* flush TLB */
	asm("mcr p15, #0, r0, c8, c7, #0");
	
	/* clear RB structures */
	memset(th->urx, 0, sizeof(RB));
	memset(th->utx, 0, sizeof(RB));
	
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
	return th;
}

int ksleep(uint32 timeout) {
	int			result;
	asm volatile (
				"push {r0}\n"
				"mov r0, %[in] \n"
				"swi #101 \n"
				"mov %[result], r0 \n"
				"pop {r0}\n"
				: [result]"=r" (result) : [in]"r" (timeout));
	/* convert from ticks */
	return result;
}

static uint32 rand(uint32 next)
{
    next = next * 1103515245 + 12345;
    return (uint32)(next / 65536) % 32768;
}

void kthread(KTHREAD *myth) {
	uint32			x;
	KPROCESS		*proc;
	KTHREAD			*th;
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
	
	ks = GETKS();
	
	for (;;) {
		/* check ring buffers for all threads */
		for (proc = ks->procs; proc; proc = proc->next) {
			for (th = proc->threads; th; th = th->next) {
				if (!th->ktx.rb) {
					/* ring buffer not instanced */
					continue;
				}
				kprintf("[kthread] reading RB\n");
				for (sz = sizeof(pkt); rb_read_nbio(&th->ktx, &pkt[0], &sz, 0); sz = sizeof(pkt)) {
					kprintf("[kthread] got pkt\n");
					/* check packet type */
					switch (pkt[0] & 0xff) {
						case 0:
							/* send message to another process:thread */
							break;
						case 1:
							/* register service */
							break;
						case 2:
							/* enumerate services */
							break;
						case 3:
							/* request shared memory from another process:thread */
							/* check for existing request (only one at a time!) */
							/* add request entry */
							/* send message to target process:thread */
							break;
						case 4:
							/* accept shared memory request from another process */
							/* grab process:thread list modify lock (can be read but not modified) */
								/* find request entry */
									/* map memory to process that is accepting request */
									/* increment ref count on physical pages */
							/* grab vmm lock on both processes (vmm for processes can not be modified) */
							/* release vmm lock and release process:thread list modify lock */
							break;
						case 5:
							/* terminate (this thread) or any other thread */
							break;
						case 6:
							/* terminate (this process) or any other process */
							break;
					}
				}
			}
		}
		kprintf("[kthread] sleeping\n");
		ksleep(ks->tpers * 10);
	}
}

void kidle() {
	for (;;) {
		asm("swi #102");
	}
}

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
	
	arm4_xrqinstall(ARM4_XRQ_RESET, &k_exphandler_reset_entry);
	arm4_xrqinstall(ARM4_XRQ_UNDEF, &k_exphandler_undef_entry);
	arm4_xrqinstall(ARM4_XRQ_SWINT, &k_exphandler_swi_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTP, &k_exphandler_abrtp_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTD, &k_exphandler_abrtd_entry);
	arm4_xrqinstall(ARM4_XRQ_IRQ, &k_exphandler_irq_entry);
	arm4_xrqinstall(ARM4_XRQ_FIQ, &k_exphandler_fiq_entry);
	
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
	*/
	kboardPrePagingInit();	
		
	kserdbg_putc('U');

	arm4_tlbsetmode(KMEMINDEX);
	/* load location of TLB */
	arm4_tlbset1((uintptr)ks->vmm.table);	/* user space */
	arm4_tlbset0((uintptr)ks->vmm.table);	/* kernel space */
	/* set that all domains are checked against the TLB entry access permissions */
	arm4_tlbsetdom(0x55555555);
	
	kserdbg_putc('F');
	/* enable TLB 0x1 and disable subpages 0x800000 */
	//arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1 | (1 << 23));
	kprintf("\ntblctrl:%x\n", arm4_tlbgetctrl());
	arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1);
	kprintf("\ntblctrl:%x\n", arm4_tlbgetctrl());
	
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
	kprintf("kthread initial-sp:%x\n", th->sp);
	th->r0 = (uint32)th;
	ks->kservthread = th;
	ks->kservproc = process;
	
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
	ks->idleth = th;
	ks->idleproc = process;
	
	/* just to get things started */
	ks->cproc = process;
	ks->cthread = th;
		
	#define KMODTYPE_ELFUSER			1
	/*
		create a task for any attached modules of the correct type
	*/

	kprintf("looking at attached modules\n");
	for (m = kPkgGetFirstMod(); m; m = kPkgGetNextMod(m)) {
		kprintf("looking at module\n");
		if (m->type == KMODTYPE_ELFUSER) {
			/* create new process */
			process = (KPROCESS*)kmalloc(sizeof(KPROCESS));
			memset(process, 0, sizeof(KPROCESS));
			ll_add((void**)&ks->procs, process);
			/* will create thread in process */
			th = kelfload(process, (uintptr)&m->slot[0], m->size);
			th->dbgname = "USERMODULE";
		}
		kprintf("looking for NEXT module..\n");
	}
	
	kprintf("....\n");

	/* 6 is 0 (fiq)  F
	   7 is 1 (irq)  I
	*/
	/* enable IRQ */
	arm4_cpsrset(arm4_cpsrget() & ~((1 << 7) | (1 << 6)));
		
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