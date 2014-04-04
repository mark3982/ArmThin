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

#define SERIAL_BASE 0x16000000
#define SERIAL_FLAG_REGISTER 0x18
#define SERIAL_BUFFER_FULL (1 << 5)

void kserdbg_putc(char c)
{
    while (*(volatile unsigned long*)(SERIAL_BASE + SERIAL_FLAG_REGISTER) & (SERIAL_BUFFER_FULL));
	
    *(volatile unsigned long*)SERIAL_BASE = c;
}

void kserdbg_puts(const char * str)
{
    while (*str != 0) {
		kserdbg_putc(*str++);
	}
}

static char* itoh(int i, char *buf)
{
	const char 	*itoh_map = "0123456789ABCDEF";
	int			n;
	int			b;
	int			z;
	int			s;
	
	if (sizeof(void*) == 4)
		s = 8;
	if (sizeof(void*) == 8)
		s = 16;
	
	for (z = 0, n = (s - 1); n > -1; --n)
	{
		b = (i >> (n * 4)) & 0xf;
		buf[z] = itoh_map[b];
		++z;
	}
	buf[z] = 0;
	return buf;
}

void __ksprintf(char *buf, const char *fmt, __builtin_va_list argp)
{
	const char 				*p;
	int 					i;
	char 					*s;
	char 					fmtbuf[256];
	int						x, y;

	//__builtin_va_start(argp, fmt);
	
	x = 0;
	for(p = fmt; *p != '\0'; p++)
	{
		if (*p == '\\') {
			switch (*++p) {
				case 'n':
					buf[x++] = '\n';
					break;
				default:
					break;
			}
			continue;
		}
	
		if(*p != '%')
		{
			buf[x++] = *p;
			continue;
		}

		switch(*++p)
			{
			case 'c':
				i = __builtin_va_arg(argp, int);
				buf[x++] = i;
				break;
			case 's':
				s = __builtin_va_arg(argp, char*);
				for (y = 0; s[y]; ++y) {
					buf[x++] = s[y];
				}
				break;
			case 'x':
				i = __builtin_va_arg(argp, int);
				s = itoh(i, fmtbuf);
				for (y = 0; s[y]; ++y) {
					buf[x++] = s[y];
				}
				break;
			case '%':
				buf[x++] = '%';
				break;
		}
	}
	
	//__builtin_va_end(argp);
	buf[x] = 0;
}

void ksprintf(char *buf, const char *fmt, ...) {
	__builtin_va_list		argp;
	
	__builtin_va_start(argp, fmt);
	__ksprintf(buf, fmt, argp);
	__builtin_va_end(argp);
}

void kprintf(const char *fmt, ...) {
	char					buf[128];
	__builtin_va_list		argp;
	
	__builtin_va_start(argp, fmt);
	__ksprintf(buf, fmt, argp);
	kserdbg_puts(buf);
	__builtin_va_end(argp);
}

void stackprinter() {
	uint32		tmp;
	uint32		*s;
	uint32		x;
	
	s = (uint32*)&tmp;
	
	for (x = 0; (((uintptr)&s[x]) & 0xFFF) != 0; ++x) {
		if (s[x] >= (uintptr)&_BOI && s[x] <= (uintptr)&_EOI) {
			kprintf("stack[%x]:%x\n", x, s[x]);
		}
	}
	kprintf("STACK-DUMP-DONE\n");
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

void k_exphandler(uint32 lr, uint32 type) {
	uint32			*t0mmio;
	uint32			*picmmio;
	uint32			swi;
	char			buf[128];
	KSTATE			*ks;
	int				x;
	KTHREAD			*kt;
	uint32			__lr, __sp, __spsr;
	uintptr			out, page;
	
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
			
			/* dont store registers on first switch */
			if (!ks->iswitch) {
				/*
					1. store register on stack in thread struct
					2. access hidden registers and store in thread struct
				*/
				kt = &ks->threads[ks->threadndx];
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
				ksprintf(buf, "kt->lr:%x threadndx:%x\n", kt->lr, ks->threadndx);
				kserdbg_puts(buf);
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
				ksprintf(buf, "<---threadndx:%x kt->pc:%x kt->pc:%x kt->lr:%x\n", ks->threadndx, kt->pc, kt->lr);
				kserdbg_puts(buf);
			}
			/*
				get next thread (if not initial switch)
			*/
			if (!ks->iswitch) {
				for (ks->threadndx = (ks->threadndx + 1) & 0xf; !ks->threads[ks->threadndx].valid; ks->threadndx = (ks->threadndx + 1) & 0xf);
			}
			
			ks->iswitch = 0;
			
			kt = &ks->threads[ks->threadndx];
			ksprintf(buf, "--->threadndx:%x kt->pc:%x kt->lr:%x\n", ks->threadndx, kt->pc, kt->lr);
			kserdbg_puts(buf);
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
			ksprintf(buf, "cpsr:%x\n", arm4_cpsrget());
			kserdbg_puts(buf);
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
			kvmm2_getphy(&ks->vmm, (uintptr)kt->vmm.table, &page);
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
			ksprintf(buf, "kt->vmm.table:%x\n", kt->vmm.table);
			kserdbg_puts(buf);
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
			ksprintf(buf, "SWI cpsr:%x spsr:%x code:%x\n", arm4_cpsrget(), arm4_spsrget(), swi);
			kserdbg_puts(buf);
			kserdbg_putc('@');
		}
		
		return;
	}
	
	if (type != ARM4_XRQ_IRQ && type != ARM4_XRQ_FIQ && type != ARM4_XRQ_SWINT) {
		/*
			Ensure, the exception return code is correctly handling LR with the
			correct offset. I am using the same return for everything except SWI, 
			which requires that LR not be offset before return.
		*/
		kserdbg_putc('\n');
		kserdbg_putc('!');
		ksprintf(buf, "lr:%x [%x]\n", lr, ((uint32*)lr)[0]);
		kserdbg_puts(buf);
		for(;;);
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
	
	ks = (KSTATE*)KSTATEADDR;
	
	kserdbg_putc('Y');
	
	arm4_xrqinstall(ARM4_XRQ_RESET, &k_exphandler_reset_entry);
	arm4_xrqinstall(ARM4_XRQ_UNDEF, &k_exphandler_undef_entry);
	arm4_xrqinstall(ARM4_XRQ_SWINT, &k_exphandler_swi_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTP, &k_exphandler_abrtp_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTD, &k_exphandler_abrtd_entry);
	arm4_xrqinstall(ARM4_XRQ_IRQ, &k_exphandler_irq_entry);
	arm4_xrqinstall(ARM4_XRQ_FIQ, &k_exphandler_fiq_entry);

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
	k_heapBMSet(&ks->hchk, (uintptr)&_BOI, (uintptr)&_EOI - (uintptr)&_BOI, 8);
	
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
	k_heapBMSet(&ks->hphy, (uintptr)&_BOI, (uintptr)&_EOI - (uintptr)&_BOI, 8);

	kserdbg_putc('D');
	
	/* vmm */
	kvmm2_baseinit(KRAMADDR);
	
	kserdbg_putc('J');
	/* map kernel image */
	kvmm2_mapmulti(&ks->vmm, 
					(uintptr)&_BOI, (uintptr)&_BOI,
					kvmm2_rndup((uintptr)&_EOI - (uintptr)&_BOI), 
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
	
	/*
		============ SCHEDULER TASK SETUP =============
	*/
	/* lets scheduler know this is going to be the first switch */
	ks->iswitch = 1;
	
	for (x = 0; x < 0x10; ++x) {
		ks->threads[x].valid = 0;
	}
	
	/* this currently executing thread and stack will be discarded when these run */
	ks->threads[0].pc = 0x80000000;
	ks->threads[0].valid = 1;
	ks->threads[0].cpsr = 0x60000000 | ARM4_MODE_USER;
	ks->threads[0].sp = 0x90001000;
	ks->threads[0].r0 = 0xa0000000;
	
	kprintf("CHECK ks->vmm.table:%x\n", ks->vmm.table);
	kprintf("[\n");
	kvmm2_init(&ks->threads[0].vmm);
	kprintf("*\n");
	kvmm2_mapsingle(&ks->threads[0].vmm, 0xa0000000, 0x16000000, TLB_C_AP_FULLACCESS);
	kprintf("!\n");
	kvmm2_allocregionat(&ks->threads[0].vmm, 1, 0x80000000, TLB_C_AP_FULLACCESS);
	kprintf("@\n");
	kvmm2_allocregionat(&ks->threads[0].vmm, 1, 0x90000000, TLB_C_AP_FULLACCESS);
	kprintf("#\n");
	kvmm2_getphy(&ks->vmm, (uintptr)ks->threads[0].vmm.table, &page);
	kprintf("+  addr:%x  getphy:%x\n", ks->threads[0].vmm.table, page);
	arm4_tlbset1(page);
	//ptwalker(ks->threads[0].vmm.table);
	//asm("mcr p15, #0, r0, c8, c7, #0");
	kprintf("copying..\n");
	/* copy some code there */
	for (x = 0; x < 1024; ++x) {
		((uint8*)0x80000000)[x] = ((uint8*)&thread1)[x];
	}
	kprintf("&\n");
	
	ks->threads[1].pc = 0x80000000;
	ks->threads[1].valid = 1;
	ks->threads[1].cpsr = 0x60000000 | ARM4_MODE_USER;
	ks->threads[1].sp = 0x90001000;
	ks->threads[1].r0 = 0xa0000000;
	kvmm2_init(&ks->threads[1].vmm);
	kvmm2_mapsingle(&ks->threads[1].vmm, 0xa0000000, 0x16000000, TLB_C_AP_FULLACCESS);
	kvmm2_allocregionat(&ks->threads[1].vmm, 1, 0x80000000, TLB_C_AP_FULLACCESS);
	kvmm2_allocregionat(&ks->threads[1].vmm, 1, 0x90000000, TLB_C_AP_FULLACCESS);
	kvmm2_getphy(&ks->vmm, (uintptr)ks->threads[1].vmm.table, &page);
	kprintf("----page[thread2]:%x\n", page);
	arm4_tlbset1(page);
	asm("mcr p15, #0, r0, c8, c7, #0");
	/* copy some code there */
	for (x = 0; x < 1024; ++x) {
		((uint8*)0x80000000)[x] = ((uint8*)&thread2)[x];
	}
	
	/* the first thread to run */
	ks->threadndx = 0x0;
	
	kserdbg_putc('Z');
	
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