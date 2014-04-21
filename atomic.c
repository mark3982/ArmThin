#include "stdtypes.h"
/*
	grabbed from:
	https://chromium.googlesource.com/chromium/src/base/+/master/atomicops_internals_arm_gcc.h
	
	but heavily modified...
*/
uint32 katomic_locktake(volatile uint32 *ptr, uint32 need_value, uint32 new_value) {
	uint32		reloop;
	uint32		old_value;
	
	reloop = 1;
	// old_value = LDREX(ptr)
	// reloop = STREX(ptr, new_value)
	asm(
				 "   ldrex %[old_value], %[ptr]\n"
				 "   cmp %[old_value], %[new_value]\n"
				 "   moveq %[reloop], #0x0\n"
				 "   beq done\n"
				 "   cmp %[need_value], %[old_value]\n"
				 "   strexeq %[reloop], %[new_value], %[ptr]\n"
				 "   done:\n"
				 : [reloop]"=r"(reloop), [ptr]"+m"(*ptr), [old_value]"=r" (old_value)
				 : [new_value]"r"(new_value), [need_value]"r"(need_value)
				 : "cc", "memory");
	/* reloop IS zero if success */
	return reloop == 0;
}

int katomic_lockspin(volatile uint32 *ptr, uint32 id) {
	while (!katomic_locktake(ptr, 0, id));
}

/* critical cpu enter (per cpu locking) */
void katomic_ccenter(volatile uint32 *ptr) {
	uint32			cpuid;
	cpuid = boardGetCPUID() + 1;			/* get our cpu ID */
	/* +1 because CPU0 is value 0 which is released value */
	katomic_lockspin(ptr, cpuid);			/* spin until we grab the lock */
}

void katomic_ccexit(volatile uint32 *ptr) {
	ptr[0] = 0;								/* drop the lock */
}

