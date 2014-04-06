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

extern uint8 _BOI;
extern uint8 _EOI;

void start(void);

/*
    This could be non-standard behavior, but as long as this resides at the top of this source and it is the
    first file used in the linking process (according to alphanumerical ordering) this code will start at the
    beginning of the .text section.
*/
void __attribute__((naked)) entry()
{
	asm("mov sp, %[ps]" : : [ps]"i" (KSTACKSTART));
	/* send to serial output */
	asm("mov r1, #0x16000000");
	asm("mov r2, #65");
	asm("str r2, [r1]");
	/* call main kernel function */
	asm("bl	start");
}

/* small chunk memory alloc/free */
void kfree(void *ptr) {
	KSTATE			*ks;
	
	ks = (KSTATE*)KSTATEADDR;

	k_heapBMFree(&ks->hchk, ptr);
}

void* kmalloc(uint32 size) {
	void			*ptr;
	KSTATE			*ks;
	uint32			_size;	
	
	ks = (KSTATE*)KSTATEADDR;
	
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
	asm("mrc p15, 0, r0, c1, c0, 0 \n\
	     mov %[ctrl], r0" : [ctrl]"=r" (ctrl));
	return ctrl;
}

void arm4_tlbsetctrl(uint32 ctrl) {
	asm("mcr p15, 0, %[ctrl], c1, c0, 0" : : [ctrl]"r" (ctrl));
}

uint32 arm4_tlbenable() {
	asm(" \
			mrc p15, 0, r0, c1, c0, 0  \n\
			orr r0, r0, #0x1 \n\
			mcr p15, 0, r0, c1, c0, 0");
}

/* physical page memory alloc/free */
void* kpalloc(uint32 size) {
	KSTATE			*ks;
	
	ks = (KSTATE*)KSTATEADDR;
	return k_heapBMAlloc(&ks->hphy, size);
}

void kpfree(void *ptr) {
	KSTATE			*ks;
	
	ks = (KSTATE*)KSTATEADDR;
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

void ksched() {
	KSTATE			*ks;
	KTHREAD			*kt;
	uint32			__lr, __sp, __spsr;
	uintptr			page;
	
	ks = (KSTATE*)KSTATEADDR;

	/* if valid process and thread then store */
	if (ks->cproc && ks->cthread) {
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
		/*
		stack viewer
		
		for (x = 0; x < 16; ++x) {
			ksprintf(buf, "stack[%x]:%x\n", x, ((uint32*)KSTACKEXC)[-x]);
			kserdbg_puts(buf);
		}
		*/
		kprintf("kt->lr:%x threadndx:%x\n", kt->lr, ks->threadndx);
		/* switch to system mode get hidden registers then switch back */
		asm("mrs r0, cpsr \n\
			 bic r0, r0, #0x1f \n\
			 orr r0, r0, #0x1f \n\
			 msr cpsr, r0 \n\
			 mov %[sp], sp \n\
			 mov %[lr], lr \n\
			 bic r0, r0, #0x1f \n\
			 orr r0, r0, #0x12 \n\
			 msr cpsr, r0 \n\
			 " : [sp]"=r" (__sp), [lr]"=r" (__lr));
		kt->sp = __sp;
		kt->lr = __lr;
		kprintf("<---threadndx:%x kt->pc:%x kt->pc:%x kt->lr:%x\n", ks->threadndx, kt->pc, kt->lr);
	}
	
	if (!ks->cproc) {
		/* initial start */
		ks->cproc = ks->procs;
		ks->cthread = ks->procs->threads;
	} else {
		if (ks->cthread) {
			/* get next thread */
			ks->cthread = ks->cthread->next;
		}
		/* if none get next process */
		if (!ks->cthread) {
			/* get next process */
			if (ks->cproc) {
				ks->cproc = ks->cproc->next;
			}
			/* if none get first process */
			if (!ks->cproc) {
				ks->cproc = ks->procs;
			}
			/* get first thread */
			if (ks->cproc) {
				ks->cthread = ks->cproc->threads;
			} else {
				ks->cthread = 0;
			}
		}
	}
	
	/* hopefully we got something or the system should deadlock */
	kt = ks->cthread;
	
	if (!kt) {
		PANIC("no-threads");
	}
	
	kprintf("--->process:%x thread:%x kt->pc:%x kt->lr:%x\n", ks->cproc, ks->cthread, kt->pc, kt->lr);
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
	kprintf("cpsr:%x\n", arm4_cpsrget());
	/* switch into system mode restore hidden registers then switch back */
	asm("mrs r0, cpsr \n\
		 bic r0, r0, #0x1f \n\
		 orr r0, r0, #0x1f \n\
		 msr cpsr, r0 \n\
		 mov sp, %[sp] \n\
		 mov lr, %[lr] \n\
		 bic r0, r0, #0x1f \n\
		 orr r0, r0, #0x12 \n\
		 msr cpsr, r0 \n\
		 " : : [sp]"r" (kt->sp), [lr]"r" (kt->lr));
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
	
	//arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1 | (1 << 23));
	kprintf("ks->cproc->vmm.table:%x\n", ks->cproc->vmm.table);
}

void k_exphandler(uint32 lr, uint32 type) {
	uint32			*t0mmio;
	uint32			*picmmio;
	uint32			swi;
	char			buf[128];
	KSTATE			*ks;
	int				x;
	KTHREAD			*kt;
	uintptr			out;
	
	ks = (KSTATE*)KSTATEADDR;
		
	kserdbg_putc('H');
	
	/*  clear interrupt in timer so it will lower its INT line
	
		if you do not clear it, an interrupt will
		be immediantly raised apon return from this
		interrupt
	*/
	
	if (type == ARM4_XRQ_IRQ) {
		picmmio = (uint32*)0x14000000;
		
		ksprintf(buf, "picmmio[PIC_IRQ_STATUS]:%x\n", picmmio[PIC_IRQ_STATUS]);
		/*
			It is possible that other pins are activated so we just check
			this one bit.
		*/
		if (picmmio[PIC_IRQ_STATUS] & 0x20) {
			t0mmio = (uint32*)0x13000000;
			t0mmio[REG_INTCLR] = 1;			/* according to the docs u can write any value */
			
			ksched();
			/* go back through normal interrupt return process */
			return;
		}
	}
	
	/*
		Get SWI argument (index).
	*/
	if (type == ARM4_XRQ_SWINT) {
		swi = ((uint32*)((uintptr)lr - 4))[0] & 0xffff;
		
		if (swi == 4) {
			kprintf("SWI cpsr:%x spsr:%x code:%x\n", arm4_cpsrget(), arm4_spsrget(), swi);
		}
		
		return;
	}
	
	if (type != ARM4_XRQ_IRQ && type != ARM4_XRQ_FIQ && type != ARM4_XRQ_SWINT) {
		/*
			Ensure, the exception return code is correctly handling LR with the
			correct offset. I am using the same return for everything except SWI, 
			which requires that LR not be offset before return.
		*/
		kprintf("!EXCEPTION\n");
		kprintf("cproc:%x cthread:%x lr:%x\n", ks->cproc, ks->cthread, lr);
		
		ll_rem((void**)&ks->cproc->threads, ks->cthread);
		ks->cthread = 0;
		ksched();
	}
	
	return;
}
	
void __attribute__((naked)) k_exphandler_irq_entry() { KEXP_TOP3; k_exphandler(lr, ARM4_XRQ_IRQ); KEXP_BOT3; }
void __attribute__((naked)) k_exphandler_fiq_entry() { KEXP_TOP3;  k_exphandler(lr, ARM4_XRQ_FIQ); KEXP_BOT3; }
void __attribute__((naked)) k_exphandler_reset_entry() { KEXP_TOP3; k_exphandler(lr, ARM4_XRQ_RESET); KEXP_BOT3; }
void __attribute__((naked)) k_exphandler_undef_entry() { KEXP_TOP3; k_exphandler(lr, ARM4_XRQ_UNDEF); KEXP_BOT3; }	
void __attribute__((naked)) k_exphandler_abrtp_entry() { KEXP_TOP3; k_exphandler(lr, ARM4_XRQ_ABRTP); KEXP_BOT3; }
void __attribute__((naked)) k_exphandler_abrtd_entry() { KEXP_TOP3; k_exphandler(lr, ARM4_XRQ_ABRTD); KEXP_BOT3; }
void __attribute__((naked)) k_exphandler_swi_entry() { KEXP_TOPSWI;   k_exphandler(lr, ARM4_XRQ_SWINT); KEXP_BOTSWI; }

void arm4_xrqinstall(uint32 ndx, void *addr) {
	char buf[32];
    uint32      *v;
    
    v = (uint32*)0x0;
	v[ndx] = 0xEA000000 | (((uintptr)addr - (8 + (4 * ndx))) >> 2);
}

void thread2(uintptr serdbgout) {
	int			x;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		((uint32*)serdbgout)[0] = 'Z';
		//asm("swi #3");
	}
}

void thread1(uintptr serdbgout) {
	int			x;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		((uint32*)serdbgout)[0] = 'Y';
		//asm("swi #4");
	}
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
	uint8			*_p;
	uintptr			x;
	
	_p = (uint8*)p;
	
	for (x = 0; x < sz; ++x) {
		_p[x] = v;
	}
}

int kelfload(KPROCESS *proc, uintptr addr, uintptr sz) {
	ELF32_EHDR			*ehdr;
	ELF32_SHDR			*shdr;
	uint32				x, y;
	uintptr				page, oldpage;
	KSTATE				*ks;
	uint8				*fb;
	KTHREAD				*th;
	
	
	kprintf("loading elf into memory space\n");
	
	ks = (KSTATE*)KSTATEADDR;
		
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
	
	ll_add((void**)&proc->threads, th);

	th->pc = ehdr->e_entry;
	th->valid = 1;
	th->cpsr = 0x60000000 | ARM4_MODE_USER;
	/* set stack */
	th->sp = 0x90001000;
	/* pass address of serial output as first argument */
	th->r0 = 0xa0000000;
	/* map serial output mmio */
	kvmm2_mapsingle(&proc->vmm, 0xa0000000, 0x16000000, TLB_C_AP_FULLACCESS);
	/* map stack page (4K) */
	kvmm2_allocregionat(&proc->vmm, 1, 0x90000000, TLB_C_AP_FULLACCESS);

	/* map address space so we can work directly with it */
	kvmm2_getphy(&ks->vmm, (uintptr)proc->vmm.table, &page);
	oldpage = arm4_tlbget1();
	arm4_tlbset1(page);
	
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
			kvmm2_allocregionat(&proc->vmm, kvmm2_rndup(shdr->sh_size), shdr->sh_addr, TLB_C_AP_FULLACCESS);
			fb = (uint8*)(addr + shdr->sh_offset);
			/* copy */
			for (y = 0; y < shdr->sh_size; ++y) {
				((uint8*)shdr->sh_addr)[y] = fb[y];
			}
		}
	}
	
	/* restore previous address space */
	arm4_tlbset1(oldpage);
}

typedef struct _KRINGBUFHDR {
	uint32				r;
	uint32				w;
} RINGBUFHDR;

/*
	the writer can write if r==w
	the writer has to wait if w < r and sz > (r - w)
    the writer has to wait if w >= r and ((mask - w) + r) < sz
	
	the reader has to wait if r == w
	the reader can read during any other condition
	
	POINTS
			- research memory barrier to prevent modification of index
			  before data is written into the buffer
			- consider two methods for sleeping.. one using a flag bit
			  for the thread/process and the other using a lock
*/

void start() {
	uint32		*t0mmio;
	uint32		*picmmio;
	KSTATE		*ks;
	int			x;
	uint32		lock;
	uint32		*utlb, *ktlb;
	char		buf[128];
	uint32		*a, *b;
	uint8		*bm;
	uint32		*tlbsub;
	uintptr		page;
	uintptr		__vmm;
	KVMMTABLE	test;
	uintptr		eoiwmods;
	KATTMOD		*m;
	KPROCESS	*process;
	
	ks = (KSTATE*)KSTATEADDR;
	
	memset(ks, 0, sizeof(KSTATE));
	
	kserdbg_putc('Y');
	
	arm4_xrqinstall(ARM4_XRQ_RESET, &k_exphandler_reset_entry);
	arm4_xrqinstall(ARM4_XRQ_UNDEF, &k_exphandler_undef_entry);
	arm4_xrqinstall(ARM4_XRQ_SWINT, &k_exphandler_swi_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTP, &k_exphandler_abrtp_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTD, &k_exphandler_abrtd_entry);
	arm4_xrqinstall(ARM4_XRQ_IRQ, &k_exphandler_irq_entry);
	arm4_xrqinstall(ARM4_XRQ_FIQ, &k_exphandler_fiq_entry);

	/* get end of image w/ mods */
	eoiwmods = kPkgGetTotalLength();
	
	kprintf("eoiwmods:%x\n", eoiwmods);
	
	/* create physical page heap */
	k_heapBMInit(&ks->hphy);
	k_heapBMInit(&ks->hchk);
	/* get a bit of memory to start with for small chunk */
	k_heapBMAddBlock(&ks->hchk, 4 * 7, KRAMADDR - (4 * 7), KCHKHEAPBSIZE);
	
	/* state structure */
	k_heapBMSet(&ks->hchk, KSTATEADDR, sizeof(KSTATE), 5);
	/* stacks (can free KSTACKSTART later) */
	k_heapBMSet(&ks->hchk, KSTACKSTART - 0x1000, 0x1000, 6);
	k_heapBMSet(&ks->hchk, KSTACKEXC - 0x1000, 0x1000, 7);
	k_heapBMSet(&ks->hchk, (uintptr)&_BOI, eoiwmods - (uintptr)&_BOI, 8);
	
	/* add block but place header in chunk heap to keep alignment */
	bm = (uint8*)k_heapBMAlloc(&ks->hchk, k_heapBMGetBMSize(KRAMSIZE - KRAMADDR, KPHYPAGESIZE));
	k_heapBMAddBlockEx(&ks->hphy, KRAMADDR, KRAMSIZE - KRAMADDR, KPHYPAGESIZE, (KHEAPBLOCKBM*)k_heapBMAlloc(&ks->hchk, sizeof(KHEAPBLOCKBM)), bm, 0);
	
	kserdbg_putc('L');	
	/* 
		remove kernel image region 
		
		This ensures it does not reside in either one. Because, KRAMADDR can change we can not
		be sure if it resides in which one or if it spans both somehow so to be safe this works
		quite well.
	*/
	k_heapBMSet(&ks->hphy, (uintptr)&_BOI, eoiwmods - (uintptr)&_BOI, 8);

	kserdbg_putc('D');
	
	/* vmm */
	kvmm2_baseinit(KRAMADDR);
	
	kserdbg_putc('J');
	/* map kernel image */
	kvmm2_mapmulti(&ks->vmm, 
					(uintptr)&_BOI, (uintptr)&_BOI,
					kvmm2_rndup(eoiwmods - (uintptr)&_BOI), 
					TLB_C_AP_PRIVACCESS | KVMM_DIRECT
	);
	/* map reverse table (ALREADY MAPPED WITH HCHK BELOW) */
	/* map interrupt table, and chunk heap (hchk) */
	kvmm2_mapmulti(&ks->vmm, 0, 0, kvmm2_rndup(KRAMADDR), TLB_C_AP_PRIVACCESS | KVMM_DIRECT | KVMM_SKIP);

	/* map serial out register, PIC, and timer */
	kvmm2_mapsingle(&ks->vmm, 0x16000000, 0x16000000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	kvmm2_mapsingle(&ks->vmm, 0x14000000, 0x14000000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	kvmm2_mapsingle(&ks->vmm, 0x13000000, 0x13000000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	
	kserdbg_putc('U');

	arm4_tlbsetmode(KMEMINDEX);
	/* load location of TLB */
	arm4_tlbset1((uintptr)ks->vmm.table);	/* user space */
	arm4_tlbset0((uintptr)ks->vmm.table);	/* kernel space */
	/* set that all domains are checked against the TLB entry access permissions */
	arm4_tlbsetdom(0x55555555);
	/* enable TLB 0x1 and disable subpages 0x800000 */
	arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1 | (1 << 23));
	
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
			kelfload(process, (uintptr)&m->slot[0], m->size);
		}
	}
	
	/* enable IRQ */
	arm4_cpsrset(arm4_cpsrget() & ~(1 << 7));
	
	/* initialize timer and PIC 
	
		The timer interrupt line connects to the PIC. You can make
		the timer interrupt an IRQ or FIQ just by enabling the bit
		needed in either the IRQ or FIQ registers. Here I use the
		IRQ register. If you enable both IRQ and FIQ then FIQ will
		take priority and be used.
	*/
	picmmio = (uint32*)0x14000000;
	picmmio[PIC_IRQ_ENABLESET] = (1<<5) | (1<<6) | (1<<7);
	
	/*
		See datasheet for timer initialization details.
	*/
	t0mmio = (uint32*)0x13000000;
	t0mmio[REG_LOAD] = 0xffffff;
	t0mmio[REG_BGLOAD] = 0xffffff;			
	t0mmio[REG_CTRL] = CTRL_ENABLE | CTRL_MODE_PERIODIC | CTRL_SIZE_32 | CTRL_DIV_NONE | CTRL_INT_ENABLE;
	t0mmio[REG_INTCLR] = ~0;		/* make sure interrupt is clear (might not be mandatory) */
	
	kserdbg_putc('K');
	kserdbg_putc('\n');
	/* infinite loop */
	for(;;);
}