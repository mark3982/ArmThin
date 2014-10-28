#include "dbgout.h"
#include "main.h"

#define TEMP_TIMER_FREQ 0x4000000

#define KEXP_TOPSWI(TYPE) \
	asm volatile ( \
		"mrc p15, 0, sp, c13, c0, 4\n" \
		"push {lr}\n" \
		"push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}\n" \
		"mrs r0, spsr\n" \
		"push {r0}\n" \
		"mov r0, lr\n" \
		"mov r1, %[type]\n" \
		"bl k_exphandler\n" \
		"pop {r0}\n" \
		"msr spsr, r0\n" \
		"pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}\n" \
		"LDM sp!, {pc}^" \
		: : [excsp]"i" (KSTARTSTATEADDR), [type]"i" (TYPE));
		 
#define KEXP_TOP3(TYPE) \
	asm volatile ( \
	"mrc p15, 0, sp, c13, c0, 4\n" \
	"sub lr, lr, #4\n" \
	"push {lr}\n" \
	"push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}\n" \
	"mrs r0, spsr\n" \
	"push {r0}\n" \
	"mov r0, lr\n" \
	"mov r1, %[type]\n" \
	"bl k_exphandler\n" \
	"pop {r0}\n" \
	"msr spsr, r0\n" \
	"pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12}\n" \
	"LDM sp!, {pc}^\n" \
	: : [ps]"i" (KSTARTSTATEADDR), [type]"i" (TYPE));

#define SERIAL_BASE 0x10009000

/*
	peripheal base and offsets relative to it
*/
#define REALVIEWPBA_PERBASE	0x1f000000	/* peripheal base */
#define REALVIEWPBA_SCUOFF	0x0000	/* snoop control unit */
#define REALVIEWPBA_GICOFF	0x0100	/* general interrupt controller */
#define REALVIEWPBA_GTIOFF	0x0200	/* global timer */
#define REALVIEWPBA_PTIOFF	0x0600	/* private timer */
#define REALVIEWPBA_GDIOFF	0x1000  /* GIC distributor */

#define REALVIEWPBA_PTIMER	0x600	/* private timers base */

extern uint8	_BOI;
extern uint8	_EOI;

typedef struct _KBOARDIF {
	uint32 volatile	*serialmmio;
	uint32 volatile	*pitmmio;
	uint8 volatile  *picmmio0;
	uint8 volatile	*picmmio1;
} KBOARDIF;


#define SET1BF(bf, off, i) bf[off + (i >> 3)] = (1 << (i & 7))
#define GET1BF(bf, off, i) bf[off + (i >> 3)]
#define GET1BV(i) (1 << (i & 7))

uint32 boardGetCPUID() {
	uint32		id;
	
	asm("mrc p15, 0, %[id], c0, c0, 5" : [id]"=r" (id));
	id = id & 0xf;
	return id;
}

int kboardCheckAndClearTimerINT() {
	KSTATE			*ks;
	KBOARDIF		*bif;
	uint32 volatile	*pt;
	KCPUSTATE		*cs;
	uint32			cpu;
	uint32			x;
	
	ks = GETKS();
	cs = GETCS();
	bif = ks->bif;
	
	cpu = boardGetCPUID();
	
	x = ((uint32*)&bif->picmmio0[0x100])[3];

	if (x == 29) {
		/* lower timer IRQ line */
		/* according to the docs u can write any value */
		pt = (uint32*)(bif->picmmio0 + REALVIEWPBA_PTIMER);
		
		/* clear pending interrupt with PIC */
		//bif->picmmio1[0x280 + (29 >> 3)] = 1 << (29 & 7);
		//bif->picmmio1[0x380 + (29 >> 3)] = 1 << (29 & 7);
		//kprintf("LOWERING\n");
		
		//for (x = 0x300; x < 0x304; ++x) {
		//	bif->picmmio1[0x100 + x] = ~0;
		//}
		((uint32*)&bif->picmmio0[0x100])[4] = x;
		
		//SET1BF(bif->picmmio1, 0x280, 29);
		//SET1BF(bif->picmmio1, 0x380, 29);
		
		pt[3] = 1;
		return 1;
	} else {
		/* 
			another interrupt, so get processid:threadid of
			and send a message
		*/
		
	}
	((uint32*)&bif->picmmio0[0x100])[4] = x;
	return 0;
}

uint32 kboardGetTimerTick() {
	KSTATE			*ks;
	KBOARDIF		*bif;
	uint32			*pt;
	
	ks = GETKS();
	bif = ks->bif;

	pt = (uint32*)((uintptr)bif->picmmio0 + REALVIEWPBA_PTIMER);
	//kprintf("diff:%x\n", pt[0] - pt[1]);
	return pt[0] - pt[1];
}

uint32 kboardGetTimerFreq() {
	return 0;
}

void kboardSetTimerTick(uint64 ticks) {
}

void kboardPostPagingInit() {
	KSTATE		*ks;
	KBOARDIF	*bif;
	uint32		*pt;
	
	ks = GETKS();
	bif = ks->bif;
	
	/* 
		6 is 0 (fiq)  F
		7 is 1 (irq)  I
	*/
	/* enable IRQ */
	arm4_cpsrset(arm4_cpsrget() & ~((1 << 7) | (1 << 6)));
	
	/* talk to CPU interface*/
	/* enable PIC for CPU 0 */
	bif->picmmio0[0x100 + 0] = 1;
	/* set priority mask for CPU 0 */
	bif->picmmio0[0x100 + 4] = 0xff;

	/* talk to actual PIC stuff */
	/* enable PIC */
	bif->picmmio1[0] = 1;	
	
	//if (GET1BF(bif->picmmio1, 0x200, 29) == GET1BV(29)) {
	
	/* enable interrupt 29 */
	SET1BF(bif->picmmio1, 0x100, 29);
	
	/* set interrupt 29 target cpu */
	bif->picmmio1[0x800 + 29] = 0;
	
	//bif->pitmmio[REG_LOAD] = KTASKTICKS;
	//bif->pitmmio[REG_BGLOAD] = KTASKTICKS;			
	//bif->pitmmio[REG_CTRL] = CTRL_ENABLE | CTRL_MODE_PERIODIC | CTRL_SIZE_32 | CTRL_DIV_NONE | CTRL_INT_ENABLE;
	//bif->pitmmio[REG_INTCLR] = ~0;		/* make sure interrupt is clear (might not be mandatory) */
	
	kprintf("bif->picmmio0:%x\n", bif->picmmio0);
	pt = (uint32*)((uintptr)bif->picmmio0 + REALVIEWPBA_PTIMER);
	pt[2] = 0;
	pt[0] = TEMP_TIMER_FREQ; 
	pt[2] = (1 << 2) | (1 << 1) | (1 << 0);
	
	ks->tpers = 0x20000;	/* about 1 second */
	
	bif->serialmmio[0] = '#';	
	
	//kprintf("@@@@@@@@@@@@@@@@@@@:%x\n", ks->hphy.fblock->size);
	
	/* wake up other CPUs */
	//((uint32*)bif->picmmio1)[0xf00 >> 2] = (1 << 24) | 0;
	return;
	//OKOK
}

void kboardSetCPUExpStack(uintptr a) {
	asm("mcr p15, 0, %[id], c13, c0, 4" : : [id]"r" (a));	
}

void kboardSetCPUState(KCPUSTATE *s) {
	asm("mcr p15, 0, %[id], c13, c0, 3" : : [id]"r" (s));	
}

KCPUSTATE *kboardGetCPUState() {
	uint32		id;
	
	asm("mrc p15, 0, %[id], c13, c0, 3" : [id]"=r" (id));
	
	return (KCPUSTATE*)id;
}

void boardSecondaryCPUBoot(KCPUSTATE *cs) {
	uint8			cpu;
	uint32 volatile	*pt;
	KBOARDIF		*bif;
	KSTATE			*ks;
	uint32			x;
	uint32			*t;
	
	ks = cs->ks;
	
	kboardSetCPUState(cs);
	
	/* set exception stack */
	kboardSetCPUExpStack(cs->excstack);
	
	cpu = boardGetCPUID();
	
	// debug to hold some cpus if wanted
	//if (cpu > 1) {
	//	for (;;) {
	//		asm("wfi");
	//	}
	//}

	bif = ks->bif;
	
	/* enable paging */
	arm4_tlbsetmode(KMEMINDEX);
	/* load location of TLB */
	arm4_tlbset1((uintptr)ks->vmm.table);	/* user space */
	arm4_tlbset0((uintptr)ks->vmm.table);	/* kernel space */
	/* set that all domains are checked against the TLB entry access permissions */
	arm4_tlbsetdom(0x55555555);
	
	arm4_setvecbase(0x0);
	
	kserdbg_putc('F');
	/* enable TLB 0x1 and disable subpages 0x800000 */
	//arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1 | (1 << 23));
	kprintf("tblctrl:%x\n", arm4_tlbgetctrl());
	arm4_tlbsetctrl(arm4_tlbgetctrl() | 0x1);
	kprintf("tblctrl:%x\n", arm4_tlbgetctrl());
	
	kprintf("CPU:%x cs:%x ks:%x\n", cpu, cs, cs->ks);
	arm4_cpsrset(arm4_cpsrget() & ~((1 << 7) | (1 << 6)));
	kprintf("ENABLED IRQ AND FIQ...\n");
	
	/* make sure it has no active proc or thread set */
	cs->cproc = 0;
	cs->cthread = 0;
	
	/* enable interrupt 29 */
	//for (x = 0; x < 32; ++x) {
	//	SET1BF(bif->picmmio1, 0x100, x);
	//}
	/* enable PIC for CPU 0 */
	bif->picmmio0[0x100 + 0] = 1;
	/* set priority mask for CPU 0 */
	bif->picmmio0[0x100 + 4] = 0xff;
	
	t = (uint32*)(REALVIEWPBA_PERBASE + REALVIEWPBA_GDIOFF);
	t[0] = 1;
	
	SET1BF(bif->picmmio1, 0x100, 29);
	
	/* setup and start private timer */
	kprintf("PTIMER:%x\n", bif->picmmio0 + REALVIEWPBA_PTIMER);
	pt = (uint32*)((uintptr)bif->picmmio0 + REALVIEWPBA_PTIMER);
	pt[2] = 0;
	pt[0] = TEMP_TIMER_FREQ; /* about 32hz */
	pt[2] = (1 << 2) | (1 << 1) | (1 << 0);
	
	for (;;) {
		//asm volatile ("wfi");
		//kprintf("WOKE UP CPU:%x\n", cpu);
	}
}

void __attribute__((naked)) boardCPUCatch() {
	KCPUSTATE				*cs;
	
	asm volatile (
		"mrc p15, 0, r1, c0, c0, 5\n"			/* cpu id */
		"mov r0, %[fixedaddr]\n"     			/* get base */
		"mov r2, #4\n"
		"mul r1, r1, r2\n"						/* get offset into base */
		"add r0, r1, r0\n"
		"ldr r0, [r0]\n"						/* get address of KCPUSTATE */
		"ldr sp, [r0]\n"						/* get address of stack */
		"mov %[cs], r0\n"
		"mov sp, %[test]\n"
		"bne boardSecondaryCPUBoot\n"
		: [cs]"+r" (cs) : [fixedaddr]"i" (KSTARTSTATEADDR), [test]"i" (KSTACKEXC)
	);
	
	for (;;);
}

void __attribute__((naked)) k_exphandler_irq_entry() { KEXP_TOP3(ARM4_XRQ_IRQ); }
void __attribute__((naked)) k_exphandler_fiq_entry() { KEXP_TOP3(ARM4_XRQ_FIQ); }
void __attribute__((naked)) k_exphandler_reset_entry() { KEXP_TOP3(ARM4_XRQ_RESET); }
void __attribute__((naked)) k_exphandler_undef_entry() { KEXP_TOP3(ARM4_XRQ_UNDEF); }	
void __attribute__((naked)) k_exphandler_abrtp_entry() { KEXP_TOP3(ARM4_XRQ_ABRTP); }
void __attribute__((naked)) k_exphandler_abrtd_entry() { KEXP_TOP3(ARM4_XRQ_ABRTD); }
void __attribute__((naked)) k_exphandler_swi_entry() { KEXP_TOPSWI(ARM4_XRQ_SWINT); }

void systemPreInitialization() {
	KSTATE			*ks;
	uintptr			eoiwmods;
	uint8			*bm;
	KCPUSTATE		*cs;
	uint32			cpucnt;
	uint32			x;
	uintptr			*offs;
	KHEAPBLOCKBM	*bh;
	
	arm4_xrqinstall(ARM4_XRQ_RESET, &k_exphandler_reset_entry);
	arm4_xrqinstall(ARM4_XRQ_UNDEF, &k_exphandler_undef_entry);
	arm4_xrqinstall(ARM4_XRQ_SWINT, &k_exphandler_swi_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTP, &k_exphandler_abrtp_entry);
	arm4_xrqinstall(ARM4_XRQ_ABRTD, &k_exphandler_abrtd_entry);
	arm4_xrqinstall(ARM4_XRQ_IRQ, &k_exphandler_irq_entry);
	arm4_xrqinstall(ARM4_XRQ_FIQ, &k_exphandler_fiq_entry);
	
	/* get number of CPU and create wanted number of states */
	cpucnt = 4;
	
	offs = (uintptr*)(KSTARTSTATEADDR);
	cs = (KCPUSTATE*)(sizeof(uintptr) * cpucnt + KSTARTSTATEADDR + sizeof(KSTATE));
	ks = (KSTATE*)(sizeof(uintptr) * cpucnt + KSTARTSTATEADDR);
	
	memset(ks, 0, sizeof(KSTATE));
	memset(cs, 0, sizeof(KCPUSTATE) * cpucnt);
	
	/* attach this address to this CPU so we can always reference it */
	kboardSetCPUState(&cs[0]);
	
	kprintf("kboardGetCPUState:%x [%x]\n", kboardGetCPUState(), &cs[0]);
	
	/* get end of image w/ mods */
	eoiwmods = kPkgGetTotalLength();
	kprintf("eoiwmods:%x\n", eoiwmods);
	
	/* create physical page heap */
	k_heapBMInit(&ks->hphy);
	k_heapBMInit(&ks->hchk);
	kprintf("HEREA\n");
	/* get a bit of memory to start with for small chunk */
	k_heapBMAddBlock(&ks->hchk, 4 * 7, KRAMADDR - (4 * 7), KCHKHEAPBSIZE);
	kprintf("HEREB\n");
	/* state structure */
	k_heapBMSet(&ks->hchk, KSTARTSTATEADDR, sizeof(KSTATE) + sizeof(KCPUSTATE) * cpucnt + sizeof(uintptr) * cpucnt, 5);
	/* stacks (can free KSTACKSTART later) */
	k_heapBMSet(&ks->hchk, KSTACKSTART - 0x1000, 0x1000, 6);
	k_heapBMSet(&ks->hchk, KSTACKEXC - 0x1000, 0x1000, 7);
	k_heapBMSet(&ks->hchk, (uintptr)&_BOI, eoiwmods - (uintptr)&_BOI, 8);
	
	/* add block but place header in chunk heap to keep alignment */
	bm = (uint8*)k_heapBMAlloc(&ks->hchk, k_heapBMGetBMSize(KRAMSIZE - KRAMADDR, KPHYPAGESIZE));
	if (!bm) {
		PANIC("COULD NOT ALLOC BM FOR HPHY ON BOOT");
	}
	bh = (KHEAPBLOCKBM*)k_heapBMAlloc(&ks->hchk, sizeof(KHEAPBLOCKBM));
	if (!bh) {
		PANIC("COULD NOT ALLOC BM HDR FOR HPHY ON BOOT");
	}
	
	kprintf("@AAA\n");
	kprintf("ks->hphy.fblock:%x ks->hphy.lock.lock:%x\n", 
					ks->hphy.fblock, 
					&ks->hphy.lock
	);
	k_heapBMAddBlockEx(&ks->hphy, KRAMADDR, KRAMSIZE - KRAMADDR, KPHYPAGESIZE, bh, bm, 0);
	kprintf("@BBB\n");
	
	kprintf("HERE\n");
	
	/* 
		remove kernel image region 
		
		This ensures it does not reside in either one. Because, KRAMADDR can change we can not
		be sure if it resides in which one or if it spans both somehow so to be safe this works
		quite well.
	*/
	kprintf("END:%x BOI:%x eoiwmods:%x\n", kvmm2_rndup(eoiwmods - (uintptr)&_BOI) * 0x1000, &_BOI, eoiwmods);
	kprintf("#@#@#@#@:%x\n", ks->hphy.fblock->used);
	k_heapBMSet(&ks->hphy, (uintptr)&_BOI, kvmm2_rndup(eoiwmods - (uintptr)&_BOI) * 0x1000, 12);
	kprintf("#@#@#@#@:%x\n", ks->hphy.fblock->used);
	
	kprintf("TEST:%x\n", ks);
	/* allocate exception stack for each CPU and set KSTATE pointer */
	for (x = 0; x < cpucnt; ++x) {
		cs[x].ks = ks;
		/* align stack on 8-byte boundary */
		cs[x].excstack = (uintptr)k_heapBMAllocBound(&ks->hchk,  1024, 3) + 1024;
		
		/* set our primary processor's exception stack (used for IRQ, FIQ, ..) */
		if (x == 0) {
			kboardSetCPUExpStack(cs[x].excstack);
		}
		
		kprintf("cs[%x]:excstack:%x\n", x, cs[x].excstack);
		if (!cs[x].excstack) {
			PANIC("COULD-NOT-ALLOC-CPU-STACK-ON-BOOT");
		}
		/* make easy quick pointer for boot code for secondary cpus */
		offs[x] = (uintptr)&cs[x];
	}
	
	/* initialize runnable threads stack for kernel */
	kstack_init(&ks->runnable);
	
	/* vmm */
	kprintf("baseinit\n");
	kvmm2_baseinit();
	
	kserdbg_putc('J');
	/* map kernel image */
	eoiwmods = kPkgGetTotalLength();
	kvmm2_mapmulti(&ks->vmm, 
					(uintptr)&_BOI, (uintptr)&_BOI,
					kvmm2_rndup(eoiwmods - (uintptr)&_BOI), 
					TLB_C_AP_PRIVACCESS | KVMM_DIRECT
	);
	/* map interrupt table, and chunk heap (hchk) -- also maps KSTATE and KCPUSTATE[] array */
	kvmm2_mapmulti(&ks->vmm, 0, 0, kvmm2_rndup(KRAMADDR), TLB_C_AP_PRIVACCESS | KVMM_DIRECT | KVMM_SKIP);

	kserdbg_putc('L');
}

/*
	This handles any special situations on startup. One good example that is
	specific to this board is the fact that two or more CPUs may start at the
	same time. Not all boards do this nor do they support the same methods. So
	that is why you find this here in the board module.
*/
void __attribute__((naked)) boardEntry() {
	asm volatile ("mrc p15, 0, r0, c0, c0, 5\n"
		 "and r0, r0, #3\n"
		 "mov r1, #0x1000\n"
		 "lsl r1, r1, #16\n"
		 "orr r1, r1, #0x9000\n"
		 "mov r2, #65\n"
		 "add r2, r2, r0\n"
		 "str r2, [r1]\n"
		 "cmp r0, #0\n"
		 "bne boardCPUCatch\n"
	);
	
	/* setup stack pointer */
	asm volatile (
		"mov sp, %[ps]\n"
		"mov r0, #0xFE\n"
		"bl	start\n"
		: : [ps]"i" (KSTACKSTART)
	);
	/* we should never return, but if we do.. */
	PANIC("RETURN-FROM-START(BOARD)");
	for (;;);
}

static uint32 rand(uint32 next)
{
    next = next * 1103515245 + 12345;
    return (uint32)(next / 65536) % 32768;
}

void kboardPrePagingInit() {
	KSTATE			*ks;
	uint32 volatile	*t0mmio;
	uint8 volatile	*a9picmmio;
	KBOARDIF		*bif;
	uint32 			*t;
	uint8			*b;
	uint32			r;
	uint32			x;
	uint32			y;
	
	ks = GETKS();
	
	bif = (KBOARDIF*)kmalloc(sizeof(KBOARDIF));
	ks->bif = bif;
	
	/* write boot address register */
	t = (uint32*)0x10000030;
	t[0] = 0x10000;
	
	/* enable general interrupt controller */
	b = (uint8*)(REALVIEWPBA_PERBASE + REALVIEWPBA_GICOFF);
	b[0] = 3;
	b[4] = 0xff;
	
	/* enable distributor then send SGI with distributor */
	t = (uint32*)(REALVIEWPBA_PERBASE + REALVIEWPBA_GDIOFF);
	t[0] = 3;
	
	/* wait to wake up other CPUs... */
		
	/* TODO: remove THESE from physical memory manager */
	/* map general interrupt controller distributor */
	kvmm2_mapmulti(				&ks->vmm, 
								REALVIEWPBA_PERBASE, 
								REALVIEWPBA_PERBASE,
								2,
								TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	/* map SERIAL */
	kprintf("MAPERMAP\n");
	bif->serialmmio = (uint32*)0x10009000;
	kvmm2_mapsingle(&ks->vmm, (uintptr)bif->serialmmio, 0x10009000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	/* map PIT */
	kvmm2_findregion(&ks->vmm, 1, 0, KMEMSIZE, TLB_C_AP_PRIVACCESS, (uintptr*)&bif->pitmmio);
	kprintf("check1:%x\n", bif->pitmmio);
	kvmm2_mapsingle(&ks->vmm, (uintptr)bif->pitmmio, 0x10011000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	/* map PIC */
	kvmm2_findregion(&ks->vmm, 1, 0, KMEMSIZE, TLB_C_AP_PRIVACCESS, (uintptr*)&bif->picmmio0);
	kprintf("check2:%x\n", bif->picmmio0);
	kvmm2_mapsingle(&ks->vmm, (uintptr)bif->picmmio0, 0x1f000000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
	kvmm2_findregion(&ks->vmm, 1, 0, KMEMSIZE, TLB_C_AP_PRIVACCESS, (uintptr*)&bif->picmmio1);
	kprintf("check3:%x\n", bif->picmmio1);
	kvmm2_mapsingle(&ks->vmm, (uintptr)bif->picmmio1, 0x1f001000, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);

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
}