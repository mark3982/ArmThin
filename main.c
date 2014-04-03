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

#define PANIC(msg) kprintf("PANIC %s LINE %x [%s]\n", __FILE__, __LINE__, msg); stackprinter(); for (;;)
#define ASSERTPANIC(cond, msg) if (!(cond)) { PANIC(msg); }

extern uint8 _BOI;
extern uint8 _EOI;

/*
	============== CONFIGURATION =============
*/
/* the intial kernel stack and the exception stack */
#define KSTACKSTART 0x2000		/* descending */
#define KSTACKEXC   0x3000		/* descending */
/* somewhere to place the kernel state structure */
#define KSTATEADDR	0x3000
/* 
	RAM is assumed to start at 0x0, but we have to leave room for a little setup code, and
	depending on how much physical memory (KRAMSIZE) we are using we might have to adjust
	KRAMADDR to give a bit more. For example if KRAMSIZE is 4GB then KRAMADDR needs to be
	larger than 1MB, perferrably 2MB at least.
*/
#define KRAMADDR	0x200000
#define KRAMSIZE	(1024 * 1024 * 8)
/* 
	kernel virtual memory size 
	
	KMEMINDEX can be 1 through 7
	1 = 2GB
	2 = 1GB
	3 = 512MB
	4 = 256MB
	5 = 128MB
	6 = 64MB
	7 = 32MB
	
	This is the maximum amount of virtual memory per kernel space.
*/
#define KMEMINDEX   3
#define KMEMSIZE	(0x1000000 << (8 - KMEMINDEX))
/* the size of a physical memory page */
#define KPHYPAGESIZE		1024
/* the block size of the chunk heap (kmalloc, kfree) */
#define KCHKHEAPBSIZE		16
/* minimum block size for chunk heap */
#define KCHKMINBLOCKSZ		(1024 * 1024)
/* block size for 1K page stack */
#define K1KPAGESTACKBSZ		1024
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

/* should be 4k in size minus header */
typedef struct _KSTACKBLOCK {
	struct _KSTACKBLOCK			*prev;
	struct _KSTACKBLOCK			*next;
	uint32						max;
	uint32						top;
} KSTACKBLOCK;

typedef struct _KSTACK {
	KSTACKBLOCK					*cur;
} KSTACK;
	
typedef struct _KVMMTABLE {
	uint32			*table;
} KVMMTABLE;

int kvmm2_allocregion(KVMMTABLE *vmm, uintptr pcnt, uintptr low, uintptr high, uint32 flags, uintptr *out);
	
typedef struct _KTHREAD {
	uint8			valid;
	uint32			r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, sp, lr, cpsr, pc;
	KVMMTABLE		vmm;
} KTHREAD;

typedef struct _KSTATE {
	/* process/thread support */
	KTHREAD			threads[0x10];
	uint8			threadndx;	
	uint8			iswitch;
	
	/* physical and heap memory management */
	KHEAPBM			hphy;			/* kernel physical page heap */
	KHEAPBM			hchk;			/* data chunk heap */
	
	/* virtual memory management */
	KVMMTABLE		vmm;			/* kernel virtual memory map */
	uint32			vmm_ucte;		/* unused coarse table entries */
	KSTACK			tstack;			/* 1K table stack */
	uint32			*vmm_rev;		/* reverse map */
} KSTATE;

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


/*
	These work with 4K pages. Optimization could be added to support 64K, 1MB, and
	16MB entries to improve TLB performance and memory consumption of TLB in memory.
	
	A 'count' refers to the number of 4K pages.
	
	Most of the functions can error out by returning a zero. This will likely leave the
	TLB table in a unfinished state. For now this is okay because we want to just halt
	the kernel and report the issue. Later, you can go back and improve the functions to
	either finish as much as they can then error out, or just report the issue and terminate
	the process. If it errors out mapping kernel memory well the calling code needs to handle
	it OR you need to halt the system with a screen of death, OR maybe you can implement
	disk swapping of virtual memory. For now these are just as simple as possible and leave
	room for improvement in performance and functionality.
*/
/* first level table */
#define TLB_FAULT			0x000		/* entry is unmapped (bits 32:2 can be used for anything) but access generates an ABORT */
#define TLB_SECTION			0x002		/* entry maps 1MB chunk */
#define TLB_COARSE			0x001		/* sub-table */
#define TLB_DOM_NOACCESS	0x00		/* generates fault on access */
#define TLB_DOM_CLIENT		0x01		/* checked against permission bits in TLB entry */
#define TLB_DOM_RESERVED	0x02		/* reserved */
#define TLB_DOM_MANAGER		0x03		/* no permissions */
#define TLB_STDFLAGS		0xc00		/* normal flags */
/* second level coarse table */
#define TLB_C_LARGEPAGE		0x1			/* 64KB page */
#define TLB_C_SMALLPAGE		0x2			/* 4KB page */
/* AP (access permission) flags for coarse table [see page 731 in ARM_ARM] */
#define TLB_C_AP_NOACCESS	(0x00<<4)	/* no access */
#define TLB_C_AP_PRIVACCESS	(0x01<<4)	/* only system access  RWXX */
#define TLB_C_AP_UREADONLY	(0x02<<4)	/* user read only  RWRX */
#define TLB_C_AP_FULLACCESS	(0x03<<4)	/* RWRW */	
/* AP (access permission) flags [see page 709 in ARM_ARM; more listed] */
#define TLB_AP_NOACCESS		(0x00<<10)	/* no access */
#define TLB_AP_PRIVACCESS	(0x01<<10)	/* only system access  RWXX */
#define TLB_AP_UREADONLY	(0x02<<10)	/* user read only  RWRX */
#define TLB_AP_FULLACCESS	(0x03<<10)	/* RWRW */

#define TLB_DOM(x)			(x<<5)		/* domain value (16 unique values) */
#define DOM_TLB(x,y)		(x<<(y*2))	/* helper macro for setting appropriate bits in domain register */

#define KVMM_DIRECT			0x80000000
#define KVMM_SKIP			0x40000000
#define KVMM_REPLACE		0x20000000

int kvmm2_mapsingle(KVMMTABLE *vmm, uintptr v, uintptr p, uint32 flags);

int kstack_initblock(KSTACKBLOCK *b) {
	uint32			x;
	/* set max and top and prev */
	b->max = (K1KPAGESTACKBSZ - sizeof(KSTACKBLOCK)) / 4;
	b->top = 0;
	
	/* clear stack */
	for (x = 0; x < b->max; ++x) {
		((uint32*)&b[1])[x] = 0;
	}
	
	return 1;
}
int kstack_empty(KSTACK *stack) {
	if (!stack->cur->top && !stack->cur->prev) {
		return 1;
	}

	return 0;
}

int kstack_pop(KSTACK *stack, uint32 *v) {
	if (!stack->cur->top && !stack->cur->prev) {
		return 0;
	}
	
	if (stack->cur->top == 0) {
		stack->cur = stack->cur->prev;
	}

	*v = ((uint32*)&stack->cur[1])[--stack->cur->top];
}

int kstack_push(KSTACK *stack, uint32 v) {
	KSTACKBLOCK				*b, *_b;
	KSTATE					*ks;
	
	ks = (KSTATE*)KSTATEADDR;
	
	b = stack->cur;
	
	if (b->top == b->max) {
		/* alloc another block */
		if (!b->next) {
			_b = (KSTACKBLOCK*)k_heapBMAllocBound(&ks->hchk, K1KPAGESTACKBSZ, 0);
			if (!_b) {
				PANIC("unable-to-create-1k-stack");
			}
		}
		/* link */
		_b->prev = b;
		_b->next = 0;
		b->next = _b;
		stack->cur = _b;
		
		kprintf("_b:%x\n", _b);
		ASSERTPANIC(_b, "KSTACK UNABLE TO CREATE NEW BLOCK");
		
		/* swap and init */
		kstack_initblock(_b);
		b = _b;
	}
	
	((uint32*)&b[1])[b->top++] = v;
}

/*
	This function does not support being called when paging is enabled. It however will
	gracefully fail if unable to initialize. It may initialize with paging on but it is
	not reliable.
*/
int kstack_init(KSTACK *stack, uintptr lowaddr) {
	KSTACKBLOCK				*b;
	KSTATE					*ks;
	uint32					x;
	
	ks = (KSTATE*)KSTATEADDR;
	/* paging is not likely up so we have to identity map */
	b = (KSTACKBLOCK*)k_heapBMAllocBound(&ks->hchk, K1KPAGESTACKBSZ, 0);
	if (!b) {
		PANIC("heap-alloc-failed");
		return 0;
	}
	
	/* link stack */
	stack->cur = b;
	b->prev = 0;
	b->next = 0;
	
	kstack_initblock(b);
	return 1;
}

uintptr kvmm2_rndup(uintptr sz) {
	return (sz / 4096) * 4096 < sz ? (sz / 4096) + 1 : sz / 4096;
}

int kvmm2_revset(uintptr p, uint32 v, uint8 opt) {
	KSTATE			*ks;
	uint32			*t;
	
	ks = (KSTATE*)KSTATEADDR;

	/* add table to reverse map */
	if (!ks->vmm_rev[p >> 20]) {
		/* 1K granularity */
		ks->vmm_rev[p >> 20] = (uint32)kmalloc(4096);
	}
	
	/* get sub-table */
	t = (uint32*)ks->vmm_rev[p >> 20];
	kprintf("REVSET %x--->%x\n", p, v);
	
	if (!opt) {
		/* keep lower and replace upper */
		t[(p >> 10) & 0x3ff] = (t[(p >> 10) & 0x3ff] & 0x3ff) | v;
		return 1;
	}
	/* keep upper and replace lower */
	t[(p >> 10) & 0x3ff] = (t[(p >> 10) & 0x3ff] & ~0x3ff) | v;
	return 1;
}

uintptr kvmm2_revget(uintptr p, uint8 opt) {
	uint32			*t;
	KSTATE			*ks;
	
	ks = (KSTATE*)KSTATEADDR;
	
	if (!ks->vmm_rev[p >> 20]) {
		kprintf("error:revget; p:%x\n", p);
		PANIC("revget-no-sub-table");
		return 0;
	}
	
	t = (uint32*)ks->vmm_rev[p >> 20];
	
	if (!opt) {
		return t[(p >> 10) & 0x3ff] & ~0x3ff;
	}
	kprintf("REVGET %x--->%x\n", p, t[(p >> 10) & 0x3ff] & 0x3ff);
	return t[(p >> 10) & 0x3ff] & 0x3ff;
}

int kvmm2_getu4k(KVMMTABLE *vmm, uintptr *o, uint32 flags) {
	uint32			x, y;
	uint32			*t, *st;
	uint32			sp;
	
	t = (uint32*)vmm->table;
	
	//asm("mov %[sp], sp" : [sp]"=r" (sp));
	//kprintf("sp:%x\n", sp);
	
	/* check 1MB coarse table entries */
	for (x = 0; x < 4096; ++x) {
		//kprintf("VERIFY:%x\n", t[1]);
		if ((t[x] & 3) != 0) {
			/* get physical address of level 2 table */
			//kprintf("t:%x t[%x]:%x\n", vmm->table, x, t[x]);
			//kprintf("------+++---- %x\n", t[1]);
			st = (uint32*)(t[x] & ~0x3ff);
			//kprintf("------+++---- %x\n", t[1]);
			if (!(flags & KVMM_DIRECT)) {
				/* translate into virtual address */
				//kprintf("before-rev:%x\n", st);
				st = (uint32*)kvmm2_revget((uintptr)st, 0);
				//kprintf("    after:%x\n", st);
				if (!st) {
					PANIC("unusual-zero-addr-for-table");
				}
			}
			/* look through table for empty entry */
			for (y = 0; y < 256; ++y) {
				if ((st[y] & 3) == 0) {
					//kprintf("t[y]:%x\n", st[y]);
					*o = (x << 20) | (y << 12);
					return 1;
				}
			}
		}
	}
	
	return 0;
}

/* get unused coarse table slot */
uint32 kvmm2_getucts(KVMMTABLE *vmm, uint32 *slot) {
	uint32			x;
	uint32			*t;
	
	t = (uint32*)vmm->table;
	
	/* check 1MB coarse table entries */
	for (x = 0; x < 4096; ++x) {
		if ((t[x] & 3) == 0) {
			*slot = x;
			return 1;
		}
	}
	
	return 0;
}

int kvmm2_get1Ktable(uintptr *o, uint32 flags) {
	KSTATE			*ks;
	uint32			*ptaddr, *vtaddr;
	uint32			x;
	uintptr			fvaddr;
	uint32			*t;
	uint32			slot;
	uint8			latemap, latemap2;
	uintptr			lm_ptaddr, lm_vtaddr, lm2_ptaddr, lm2_vtaddr;
	
	ks = (KSTATE*)KSTATEADDR;
	
	t = ks->vmm.table;
	latemap = 0;
	
	/* we only need to know to use direct or not */
	if (flags & KVMM_DIRECT) {
		flags = KVMM_DIRECT;
	} else {
		flags = 0;
	}
	
	//kprintf("---enter---\n");
	// check table stack.. is empty?
	if (kstack_empty(&ks->tstack)) {
		// check ucte (if less than 2 left)
		if (ks->vmm_ucte < 2) {
			kprintf("INCREASING UCTE\n");
			/* get free table slot */
			kvmm2_getucts(&ks->vmm, &slot);
			//kprintf("slot:%x\n", slot);
			/* are we at the end of our address space */
			if ((slot << 20) > KMEMSIZE) {
				/* going outside of kernel address space */
				//kprintf("slot:%x\n", slot);
				PANIC("(slot << 30) > KMEMSIZE");
				return 0;
			}
			ptaddr = (uint32*)k_heapBMAllocBound(&ks->hphy, 1024, 10);
			t[slot] = (uintptr)ptaddr | TLB_COARSE;
			if (!(flags & KVMM_DIRECT)) {
				/* get empty 1K entry address */
				if (!kvmm2_getu4k(&ks->vmm, (uintptr*)&vtaddr, flags)) {
					PANIC("getu4k-failed");
				}
				for (x = 0; x < 256; ++x) {
					vtaddr[x] = 0;
				}
				/* set reverse map */
				kvmm2_revset((uintptr)ptaddr, (uintptr)vtaddr, 0);
				/* map page into space */
				kvmm2_mapsingle(&ks->vmm, (uintptr)vtaddr, (uintptr)ptaddr, flags | TLB_C_AP_PRIVACCESS);
			} else {
				vtaddr = ptaddr;
				kvmm2_revset((uintptr)ptaddr, (uintptr)vtaddr, 0);
				latemap = 1;
				lm_ptaddr = (uintptr)ptaddr;
				lm_vtaddr = (uintptr)vtaddr;
				for (x = 0; x < 256; ++x) {
					vtaddr[x] = 0;
				}
			}
			
			/* clear the table */
			for (x = 0; x < 256; ++x) {
				vtaddr[x] = 0;
			}
			
			ks->vmm_ucte += 256;
		}
		
		kprintf("INCREASING 1K PAGE STACK\n");
		/* alloc 4K but on a 4K boundary */
		ptaddr = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096, 12);
		if (!(flags & KVMM_DIRECT)) {
			/* map into memory and get virtual address */
			/* get empty 4K entry address */
			//kprintf("CHECK:%x\n", ks->vmm.table[1]);
			kvmm2_getu4k(&ks->vmm, (uintptr*)&vtaddr, flags);
			//kprintf("4k:%x p:%x\n", vtaddr, ptaddr);
			/* map page into space */
			//kprintf("$$$$$$1 v:%x p:%x\n", vtaddr, ptaddr);
			kvmm2_mapsingle(&ks->vmm, (uintptr)vtaddr, (uintptr)ptaddr, flags | TLB_C_AP_PRIVACCESS);
			//kprintf("$$$$$$2\n");
			latemap2 = 0;
		} else {
			//kprintf("latemap2!!!!\n");
			latemap2 = 1;
			vtaddr = ptaddr;
			lm2_vtaddr = (uintptr)vtaddr;
			lm2_ptaddr = (uintptr)ptaddr;
		}
		// add 4-1K pages to stack
		for (x = 0; x < 4; ++x) {
			kprintf("ok1\n");
			kvmm2_revset((uintptr)ptaddr + x * 0x400, (uintptr)vtaddr + x * 0x400, 0);
			kprintf("ok2\n");
			kstack_push(&ks->tstack, (uintptr)vtaddr + 0x400 * x);
		}
		
		/* late map */
		if (latemap2) {
			kprintf("   ....latemap2 [enter]\n");
			kvmm2_mapsingle(&ks->vmm, lm2_vtaddr, lm2_ptaddr, flags | TLB_C_AP_PRIVACCESS);
			kprintf("   ....latemap2 [exit]\n");
		}
	}
	
	if (latemap) {
		kprintf("   ....latemap1 [enter]\n");
		if (!kvmm2_mapsingle(&ks->vmm, lm_vtaddr, lm_ptaddr, flags | TLB_C_AP_PRIVACCESS)) {
			PANIC("late-map-failed\n");
		}
		kprintf("   ....latemap1 [exit]\n");
	}
	//kprintf("flags:%x\n", flags);
	// pop one (return value)
	kstack_pop(&ks->tstack, (uint32*)o);
	return 1; 
}

int kvmm2_getphy(KVMMTABLE *vmm, uintptr v, uintptr *o) {
	uint32			*t;
	
	t = vmm->table;
	/* check not empty level one table entry */
	if ((t[v >> 20] & 3) == 0) {
		return 0;
	}
	
	/* get sub table physical address */
	t = (uint32*)(t[v >> 20] & ~0x3ff);
	/* do reverse lookup to get virtual address */
	t = (uint32*)kvmm2_revget((uintptr)t, 0);
	/* get index into subtable, then drop flags off end of entry */
	/* 
		also keep lower 12 bits of v address because we assume
		that they want the physical address not the actual physical
		page address
	*/
	*o = (t[(v >> 12) & 0xff] & ~0xfff) | (v & 0xfff); 
	return 1;
}

int kvmm2_findregion(KVMMTABLE *vmm, uintptr tc, uintptr low, uintptr high, uint32 flags, uintptr *out) {
	uint32			*t, *st;
	KSTATE			*ks;
	uintptr			target;
	uintptr			c;
	uintptr			start;
	
	ks = (KSTATE*)KSTATEADDR;
	
	kprintf("kvmm2_findregion; tc:%x low:%x high:%x\n", tc, low, high);
	
	/* move low up if needed onto page boundary */
	low = low & 0xfff > 0 ? (low & ~0xfff) + 0x1000 : low & ~0xfff;
	/* drop high down if needed onto page boundary */
	high = (high & ~0xfff) + 0x1000;
	
	t = vmm->table;
	c = 0;
	//kprintf("t:%x\n", t);
	for (target = low, start = low; target < high; target += 0x1000) {
		//kprintf("   target:%x\n", t[target >> 20]);
		if ((t[target >> 20] & 3) == 0) {
			c += 256;
			if (c >= tc) {
				//kprintf(" 1--start:%x c:%x\n", start, c);
				*out = start;
				return 1;
			}
			/* skip */
			target += 255 * 0x1000;
		} else {
			st = (uint32*)(t[target >> 20] & ~0x3ff);
			if (!(flags & KVMM_DIRECT)) {
				/* translate sub-table physical address into virtual */
				st = (uint32*)kvmm2_revget((uintptr)st, 0);
			}
			
			if ((st[(target >> 12) & 0xFF] & 3) == 0) {
				c += 1;
				if (c >= tc) {
					//kprintf("  2--start:%x c:%x\n", start, c);
					*out = start;
					return 1;
				}
			} else {
				start = target + 0x1000;
				c = 0;
				//kprintf("   ..start:%x\n", start);
			}
		}
	}
	kprintf("can not find region\n");
	return 0;
}

int kvmm2_allocregion(KVMMTABLE *vmm, uintptr pcnt, uintptr low, uintptr high, uint32 flags, uintptr *out) {
	uint32			x;
	uintptr			p;
	KSTATE			*ks;
	
	ks = (KSTATE*)KSTATEADDR;
	
	if (!high) {
		high = low + pcnt * 0x1000;
	}
	
	if (!kvmm2_findregion(vmm, pcnt, low, high, flags, out)) {
		return 0;
	}
	
	kprintf("out:%x\n", out[0]);
	
	for (x = 0; x < pcnt; ++x) {
		p = (uintptr)k_heapBMAllocBound(&ks->hphy, 0x1000, 12);
		if (!p) {
			PANIC("heap-alloc-failed");
			return 0;
		}

		if (!kvmm2_mapsingle(vmm, out[0] + x * 0x1000, p, flags)) {
			PANIC("map-single-failed");
			return 0;
		}
		kprintf("mapped %x -> %x\n", out[0] + x * 0x1000, p);
	}
	
	return 1;
}

int kvmm2_allocregionat(KVMMTABLE *vmm, uintptr pcnt, uintptr start, uint32 flags) {
	uintptr				tmp;
	return kvmm2_allocregion(vmm, pcnt, start, 0, flags, &tmp);
}

int kvmm2_mapsingle(KVMMTABLE *vmm, uintptr v, uintptr p, uint32 flags) {
	uint32			*t;
	uint32			*st;
	KSTATE			*ks;
	uintptr			phy;
	char			buf[128];
	uint32			x;
	
	ks = (KSTATE*)KSTATEADDR;
	
	t = vmm->table;
	
	/* see if we need a page */
	if ((t[v >> 20] & 3) == 0) {
		/* maybe split this function into one to init/prep and one to actually get page */
		kvmm2_get1Ktable((uintptr*)&st, flags);
		/* TODO: put table back into stack if not used */
	}
	
	if ((t[v >> 20] & 3) == 0) {
		/* update coarse table entries (ONLY for kernel space) */
		if (ks->vmm.table == t) {
			ks->vmm_ucte += 256;
		}
		if (!(flags & KVMM_DIRECT)) {
			/* reverse lookup table (use physical address) [keep st same] */
			if (!kvmm2_getphy(&ks->vmm, (uintptr)st, (uintptr*)&phy)) {
				PANIC("getphyfailed");
			}
			/* set table */
			if (phy == 0x20a000) {
				kprintf("=========================GOTIT (from:%x)\n", st);
			}
			t[v >> 20] = phy | TLB_COARSE;
			kprintf("clearing 1k table %x\n", st);
			for (x = 0; x < 256; ++x) {
				st[x] = 0;
			}
			/* st is already virtual */
		} else {
			/* ks->vmm not ready so were in direct mode */
			t[v >> 20] = (uintptr)st | TLB_COARSE;
			for (x = 0; x < 256; ++x) {
				st[x] = 0;
			}
			/* st is already virtual/direct */
		}
	} else {
		st = (uint32*)(t[v >> 20] & ~0x3ff);
		if (!(flags & KVMM_DIRECT)) {
			/* get virtual from physical using reverse lookup */
			//kprintf("page:%x v:%x\n", st, v);
			//if ((uintptr)st == 0x209400) {
			//	PANIC("CHECKME");
			//}
			st = (uint32*)kvmm2_revget((uintptr)st, 0);
			if (!st) {
				PANIC("revgetfailed");
			}
		}
	}
	
	//kprintf("@@@  st:%x\n", st);
	/* access second level table */
	if ((st[(v >> 12) & 0xff] & 3) != 0) {
		kprintf("  inside\n");
		if (!(flags & KVMM_REPLACE)) {
			if (!(flags & KVMM_SKIP)) {
				ksprintf(buf, "something already here; table:%x virtual:%x val:%x\n", st, v, st[(v>>12)&0xff]);
				kserdbg_puts(buf);
				return 0;
			} else {
				/* just skip it with no error */
				return 2;
			}
		}
	}
	//kprintf("   outside\n");
	/* update coarse table entries.. we used one (ONLY for kernel space) */
	if (ks->vmm.table == t) {
		ks->vmm_ucte -= 1;
	}

	kprintf("mapped tbl:[%x] %x -> %x [flags:%x]\n", t, v, p, flags & 0xfff);
	
	flags = flags & 0xfff;
	st[(v >> 12) & 0xff] = p | flags | TLB_C_SMALLPAGE;
	
	return 1;
}

int kvmm2_mapmulti(KVMMTABLE *vmm, uintptr v, uintptr p, uintptr c, uint32 flags) {
	uintptr			x;
	int				ret;
	
	for (x = 0; x < c; ++x) {
		ret = kvmm2_mapsingle(vmm, v + 0x1000 * x, p + 0x1000 * x, flags);  
		if (!ret) {
			PANIC("mapmulti-failed");
			return 0;
		}
	}
	
	return 1;
}

void ptwalker(uint32 *t) {
	uint32		x, y;
	uint32		*st;
	
	kprintf("ptwalker()..\n");
	for (x = 0; x < 4096; ++x) {
		if (t[x] != 0) {
			kprintf("  [i:%x p:%x]\n", x, t[x]);
			st = (uint32*)(t[x] & ~0x3ff);
			st = (uint32*)kvmm2_revget((uintptr)st, 0);
			kprintf("             [v:%x]\n", st);
			for (y = 0; y < 256; ++y) {
				if (st[y] != 0) {
					kprintf("      [%x]:%x\n", y, st[y]);
				}
			}
		}
	}
}

int kvmm2_init(KVMMTABLE *t) {
	uint32			x;
	KSTATE			*ks;
	uintptr			addr;
	 
	ks = (KSTATE*)KSTATEADDR;
	
	t->table = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096 * 4, 14);
	
	kprintf("t->table(PHYSICAL):%x\n", t->table);
	
	if (!ks->vmm.table) {
		PANIC("vmm-table-alloc-failed");
		return 0;
	}	
	
	//kvmm2_findregion(&ks->vmm, 4, 0, KMEMSIZE, 0, &addr);
	if (!kvmm2_findregion(&ks->vmm, 4, 0, KMEMSIZE, 0, &addr)) {
		PANIC("findregionfailed");
		return 0;
	}
	
	kprintf("vmm init table:%x\n", t->table);
	
	if (!kvmm2_mapmulti(&ks->vmm, addr, (uintptr)t->table, 4, TLB_C_AP_PRIVACCESS)) {
		PANIC("vmm-table-map-failed");
		return 0;
	}
	//asm("mcr p15, #0, r0, c8, c7, #0");
	//5am-8am
	kprintf("zeroing t->table;    v:%x    p:%x\n", addr, t->table);
	t->table = (uint32*)(addr);
	
	uintptr			o;
	
	kvmm2_getphy(&ks->vmm, (uintptr)ks->vmm.table, &o);
	//kvmm2_getphy(&ks->vmm, (uintptr)t->table, &o);
	kprintf("ks->vmm.table; v:%x   p:%x\n", ks->vmm.table, o);
	
	//ptwalker(ks->vmm.table);
	for (x = 0; x < 4096; ++x) {
		t->table[x] = 0;
	}
	kprintf("exiting\n");
	return 1;
}

int kvmm2_init_revtable() {
	uint32		x;
	KSTATE		*ks;
	
	ks = (KSTATE*)KSTATEADDR;

	ks->vmm_rev = (uint32*)k_heapBMAllocBound(&ks->hchk, 4096 * 4, 0);
	
	if (!ks->vmm_rev) {
		PANIC("vmm_rev.alloc.failed");
	}
	
	for (x = 0; x < 4096; ++x) {
		ks->vmm_rev[x] = 0;
	}
	
	return 1;
}

int kvmm2_baseinit(uintptr lowaddr) {
	KSTATE			*ks;
	uint32			x;
	char			buf[128];
	
	ks = (KSTATE*)KSTATEADDR;

	ks->vmm_ucte = 0;
	
	/* initialize kernel page table */
	ks->vmm.table = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096 * 4, 14);
	
	for (x = 0; x < 4096; ++x) {
		ks->vmm.table[x] = 0;
	}
	
	/* initialize reverse map table */
	kvmm2_init_revtable();

	/* initial table stack*/
	kstack_init(&ks->tstack, lowaddr);
	
	/* map kernel page table (when we enable paging we still want to access it) */
	kvmm2_mapmulti(&ks->vmm, (uintptr)ks->vmm.table, (uintptr)ks->vmm.table, 4, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
}

/* ----------------------------------- */
		
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
	
	//kprintf("ks->vmm_rev[0x20a000>>20]:%x\n", ks->vmm_rev[0x20a000 >> 20]);
	//for(;;);
	
	//ksprintf(buf, "test:%x\n", a[0x204000 & ~0xFFFFF]);
	//kserdbg_puts(buf);
	//for(;;);
	
	/*
		Something is accessing this memory location, and I have to figure out
		what it is.
	*/
	//ks->vmm.table[352] = (352 << 20) | TLB_AP_PRIVACCESS | TLB_SECTION;
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
	
	//207000
	
	kserdbg_putc('G');
	
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
		kprintf("copying byte\n");
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