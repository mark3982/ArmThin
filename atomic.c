#include "stdtypes.h"
#include "main.h"
#include "atomic.h"

/*
	original code from:
		https://chromium.googlesource.com/chromium/src/base/+/master/atomicops_internals_arm_gcc.h
	
	but modified heavily..
*/
uint32 katomic_locktake32(volatile uint32 *ptr, uint32 need_value, uint32 new_value) {
	uint32 volatile		reloop;
	uint32 volatile		old_value;
	
	reloop = 1;
	// old_value = LDREX(ptr)
	// reloop = STREX(ptr, new_value)
	asm volatile (
				 "   ldrex %[old_value], %[ptr]\n"
				 "   cmp %[old_value], %[new_value]\n"
				 "   moveq %[reloop], #0x0\n"
				 "   beq done%=\n"
				 "   cmp %[need_value], %[old_value]\n"
				 "   strexeq %[reloop], %[new_value], %[ptr]\n"
				 "   done%=:\n"
				 : [reloop]"=r"(reloop), [ptr]"+m"(*ptr), [old_value]"=r" (old_value)
				 : [new_value]"r"(new_value), [need_value]"r"(need_value)
				 : "cc", "memory");
	/* reloop IS zero if success */
	return reloop == 0;
}

int katomic_lockspin32(volatile uint32 *ptr, uint32 id) {
	while (!katomic_locktake32(ptr, 0, id));
}

int katomic_lockspin_yield(volatile KATOMIC_CCLOCK *ptr, uint32 id) {
	uint32 volatile cyclecnt;

	cyclecnt = 0;
	while (!katomic_locktake32(&ptr->lock, 0, id) || ptr->lock != id) {
		asm("swi %[code]" : : [code]"i" (KSWI_YEILD));
		cyclecnt++;
		if (cyclecnt > 400) {
			kprintf("WHOA-THIS-YIELD-LOCK-TAKING-A-LONG-TIME lock:%x id:%x myid:%x", ptr, ptr->lock, id);
			for (;;);
		}
	}
	//kprintf("ATOMIC-LOCK-AQUIRE lock:%x owner:%x myid:%x\n", ptr, ptr->lock, id);
	ptr->cnt++;
}

typedef int (*KATOMIC_LOCKSPIN8NR)(volatile uint8 *ptr, uint8 id);

/*
	non-re-entrant lock for kernel space
*/
int katomic_lockspin_wfe8nr(volatile uint8 *ptr, uint8 id) {
	while (!katomic_locktake8(ptr, 0, id) || *ptr != id) {
		asm volatile ("wfe");
	}
}

int katomic_lockspin_wfe(volatile KATOMIC_CCLOCK *ptr, uint32 id) {
	uint32 volatile cyclecnt;

	cyclecnt = 0;
	while (!katomic_locktake32(&ptr->lock, 0, id) || ptr->lock != id) {
		asm volatile ("wfe");
		cyclecnt++;
		if (cyclecnt > 200) {
			kprintf("WHOA-THIS-WFE-LOCK-TAKING-A-LONG-TIME lock:%x id:%x myid:%x", ptr, ptr->lock, id);
			for (;;);
		}
	}
	//kprintf("ATOMIC-LOCK-AQUIRE lock:%x owner:%x myid:%x\n", ptr, ptr->lock, id);
	ptr->cnt++;
}

/* critical cpu enter (per cpu locking) */
void katomic_ccenter(volatile uint32 *ptr) {
	uint32			cpuid;
	
	cpuid = boardGetCPUID() + 1;			/* get our cpu ID */
	/* +1 because CPU0 is value 0 which is released value */
	katomic_lockspin32(ptr, cpuid);			/* spin until we grab the lock */
}

void katomic_ccenter_yield(volatile KATOMIC_CCLOCK *ptr) {
	KCPUSTATE		*cs;
	
	cs = GETCS();
	
	katomic_lockspin_yield(ptr, (uint32)cs->cthread);
}

/* critical cpu enter (per cpu locking) */
void katomic_ccenter_wfe(volatile KATOMIC_CCLOCK *ptr) {
	uint32			cpuid;
	
	cpuid = boardGetCPUID() + 1;				/* get our cpu ID */
	/* +1 because CPU0 is value 0 which is released value */
	katomic_lockspin_wfe(ptr, cpuid);			/* spin until we grab the lock */
}


void katomic_ccexit_yield(volatile KATOMIC_CCLOCK *ptr) {
	KCPUSTATE		*cs;
	
	cs = GETCS();

	if (ptr->lock != (uint32)cs->cthread) {
		kprintf("lock:%x owner:%x this-thread:%x\n", ptr, ptr->lock, cs->cthread);
		PANIC("RELEASE-OF-UNOWNED-WFE-LOCK");
	}
	
	ptr->cnt--;
	
	if (ptr->cnt == 0) {
		ptr->lock = 0;
	}
}

void katomic_ccexit_sev(volatile KATOMIC_CCLOCK *ptr) {
	uint32			cpuid;
	
	cpuid = boardGetCPUID() + 1;			/* get our cpu ID */
	
	/* a little sanity check.. do we own this lock? */
	if (ptr->lock != cpuid) {
		kprintf("lock:%x owner:%x this-cpu:%x\n", ptr, ptr->lock, cpuid);
		PANIC("RELEASE-OF-UNOWNED-WFE-LOCK");
	}
	
	ptr->cnt--;										/* decrement reference count */
	
	//kprintf("ATOMIC-LOCK-RELEASE lock:%x owner:%x\n", ptr, ptr->lock);
	if (ptr->cnt == 0) {
		ptr->lock = 0;								/* drop the lock */
		asm volatile ("sev");
	}
}

