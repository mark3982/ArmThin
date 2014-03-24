/*
	http://wiki.osdev.org/User:Pancakes/BitmapHeapImplementation
	
	I just placed all the structs and prototypes into a seperate header.
	
	You need to create this header, and place the implementation into another
	source file named for example kheap_bm.c. Then compile and link that with
	this source file.
*/
#include "kheap.h"

#ifdef B64
typedef unsigned long long  uintptr;
#else
typedef unsigned int		uintptr;
#endif
typedef unsigned long long  uint64;
typedef unsigned int		uint32;
typedef unsigned char		uint8;
typedef unsigned short		uint16;

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

/*
	============== CONFIGURATION =============
*/
/* the intial kernel stack and the exception stack */
#define KSTACKSTART 0x2000		/* descending */
#define KSTACKEXC   0x3000		/* descending */
/* somewhere to place the kernel state structure */
#define KSTATEADDR	0x3000
/* the address of the start of usable RAM and it's length in bytes */
#define KRAMADDR	0x3000
#define KRAMSIZE	(1024 * 1024 * 8)
/* the size of a physical memory page */
#define KPHYPAGESIZE		4096
/* the block size of the chunk heap (kmalloc, kfree) */
#define KCHKHEAPBSIZE		16
/* minimum block size for chunk heap */
#define KCHKMINBLOCKSZ		(1024 * 1024)
/*
	============================================
*/

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
	
	for (z = 0, n = 8; n > -1; --n)
	{
		b = (i >> (n * 4)) & 0xf;
		buf[z] = itoh_map[b];
		++z;
	}
	buf[z] = 0;
	return buf;
}

void ksprintf(char *buf, const char *fmt, ...)
{
	const char 				*p;
	__builtin_va_list		argp;
	int 					i;
	char 					*s;
	char 					fmtbuf[256];
	int						x, y;

	__builtin_va_start(argp, fmt);
	
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
				s = __builtin_va_arg(argp, char *);
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
	
	__builtin_va_end(argp);
	buf[x] = 0;
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

void arm4_loadtlb(uintptr base) {
	asm("mcr p15, 0, %[tlb], c2, c0, 0" : : [tlb]"r" (base));
}

typedef struct _KTHREAD {
	uint8			valid;
	uint32			r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, cpsr, pc;
} KTHREAD;

typedef struct _KSTATE {
	KTHREAD			threads[0x10];
	uint8			threadndx;	
	uint8			iswitch;
	KHEAPBM			hphy;		/* physical page heap */
	KHEAPBM			hchk;		/* data chunk heap */
} KSTATE;

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
		if (size < KCHKMINBLOCKSZ) {
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
		/* try allocation once more, should succeed */
		k_heapBMAddBlock(&ks->hchk, (uintptr)ptr, _size, KCHKHEAPBSIZE);
		ptr = k_heapBMAlloc(&ks->hchk, size);
	}
	
	return ptr;
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
		kserdbg_puts(buf);
		
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
				kt->r0 = ((uint32*)KSTACKEXC)[-2];
				kt->r1 = ((uint32*)KSTACKEXC)[-3];
				kt->r2 = ((uint32*)KSTACKEXC)[-4];
				kt->r3 = ((uint32*)KSTACKEXC)[-5];
				kt->r4 = ((uint32*)KSTACKEXC)[-6];
				kt->r5 = ((uint32*)KSTACKEXC)[-7];
				kt->r6 = ((uint32*)KSTACKEXC)[-8];
				kt->r7 = ((uint32*)KSTACKEXC)[-9];
				kt->r8 = ((uint32*)KSTACKEXC)[-10];
				kt->r9 = ((uint32*)KSTACKEXC)[-11];
				kt->r10 = ((uint32*)KSTACKEXC)[-12];
				kt->r11 = ((uint32*)KSTACKEXC)[-13];
				kt->r12 = ((uint32*)KSTACKEXC)[-14];
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
					 orr r0, r0, #0x1f \n\
					 msr cpsr, r0 \n\
					 mov %[sp], sp \n\
					 mov %[lr], lr \n\
					 lsr r0, r0, #5 \n\
					 lsl r0, r0, #5 \n\
					 orr r0, r0, #0x12 \n\
					 msr cpsr, r0 \n\
					 " : [sp]"=r" (__sp), [lr]"=r" (__lr));
				kt->sp = __sp;
				kt->lr = __lr;
				ksprintf(buf, "<---threadndx:%x kt->sp:%x kt->pc:%x kt->lr:%x\n", ks->threadndx, kt->pc, kt->lr);
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
			((uint32*)KSTACKEXC)[-2] = kt->r0;
			((uint32*)KSTACKEXC)[-3] = kt->r1;
			((uint32*)KSTACKEXC)[-4] = kt->r2;
			((uint32*)KSTACKEXC)[-5] = kt->r3;
			((uint32*)KSTACKEXC)[-6] = kt->r4;
			((uint32*)KSTACKEXC)[-7] = kt->r5;
			((uint32*)KSTACKEXC)[-8] = kt->r6;
			((uint32*)KSTACKEXC)[-9] = kt->r7;
			((uint32*)KSTACKEXC)[-10] = kt->r8;
			((uint32*)KSTACKEXC)[-11] = kt->r9;
			((uint32*)KSTACKEXC)[-12] = kt->r10;
			((uint32*)KSTACKEXC)[-13] = kt->r11;
			((uint32*)KSTACKEXC)[-14] = kt->r12;
			((uint32*)KSTACKEXC)[-15] = kt->cpsr;
			/* switch into system mode restore hidden registers then switch back */
			asm("mrs r0, cpsr \n\
				 orr r0, r0, #0x1f \n\
				 msr cpsr, r0 \n\
				 mov sp, %[sp] \n\
				 mov lr, %[lr] \n\
				 lsl r0, r0, #5 \n\
				 lsr r0, r0, #5 \n\
				 orr r0, r0, #0x12 \n\
				 msr cpsr, r0 \n\
				 " : : [sp]"r" (kt->sp), [lr]"r" (kt->lr));
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
		kserdbg_putc('!');
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

void thread2() {
	int			x;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		kserdbg_putc('B');
	}
}

void thread1() {
	int			x;

	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		kserdbg_putc('A');
		asm("swi #4");
	}
}

void start() {
	uint32		*t0mmio;
	uint32		*picmmio;
	KSTATE		*ks;
	int			x;
	uint32		lock;
	uint32		*tlb;
	
	ks = (KSTATE*)KSTATEADDR;
	
	kserdbg_putc('Y');
	
	/*
		========== CREATE PHYSICAL PAGE HEAP AND CHUNK HEAP ======
		The physical page heap deals with actual physical pages (higher level memory management)
		which would normally be of 4K in size, while the chunk heap deals with much smaller
		allocations from 16 bytes or larger. 
		
		I think it is niftty that we can essentially use the same implementation to manage 
		physical pages as we do for normal kmalloc calls.
	*/
	/* create physical page heap */
	k_heapBMInit(&ks->hphy);
	/* we lose about 2K (doing nothing) but if KPHYPAGESIZE was 2048 we would not but slower */
	k_heapBMAddBlock(&ks->hphy, KRAMADDR, KRAMSIZE, KPHYPAGESIZE);
	/* create chunk heap (small allocations) */
	k_heapBMInit(&ks->hchk);
	/* hs->hchk wil have blocks added on demand.. so we do nothing right now (see kmalloc) */

	tlb = (uint32*)k_heapBMAllocBound(&ks->hphy, 1024 * 16, 14); 
	
	#define TLB_SECTION			0x002
	#define TLB_CACHE			0x000
	#define TLB_DOMAIN			0x000
	#define TLB_ACCESS			0xc00;
	
	for (x = 0; x < 1024; ++x) {
		tlb[x] = (x << 20) | TBL_ACCESS | TLB_SECTION;
	}
	
	arm4_loadtlb((uintptr)tlb);
	
	
	/* testing something GCC specific (built-in atomic locking) */
	//while (__sync_val_compare_and_swap(&lock, 0, 1));
	
	/*
		============ SCHEDULER TASK SETUP =============
	*/
	/* lets scheduler know this is going to be the first switch */
	ks->iswitch = 1;
	
	for (x = 0; x < 0x10; ++x) {
		ks->threads[x].valid = 0;
	}
	
	/* this currently executing thread and stack will be discarded when these run */
	ks->threads[0].pc = (uint32)&thread1;
	ks->threads[0].valid = 1;
	ks->threads[0].cpsr = 0x60000000 | ARM4_MODE_USER;
	ks->threads[0].sp = 0x5000;
	
	ks->threads[1].pc = (uint32)&thread2;
	ks->threads[1].valid = 1;
	ks->threads[1].cpsr = 0x60000000 | ARM4_MODE_USER;
	ks->threads[1].sp = 0x7000;
	
	/* the first thread to run */
	ks->threadndx = 0x0;

	arm4_xrqinstall(ARM4_XRQ_RESET, &k_exphandler_reset_entry);
	arm4_xrqinstall(ARM4_XRQ_UNDEF, &k_exphandler_undef_entry);
	arm4_xrqinstall(ARM4_XRQ_SWINT, &k_exphandler_swi_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTP, &k_exphandler_abrtp_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTD, &k_exphandler_abrtd_entry);
	arm4_xrqinstall(ARM4_XRQ_IRQ, &k_exphandler_irq_entry);
	arm4_xrqinstall(ARM4_XRQ_FIQ, &k_exphandler_fiq_entry);
	
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