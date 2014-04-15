#include "stdtypes.h"

/*
	grabbed from:
	https://chromium.googlesource.com/chromium/src/base/+/master/atomicops_internals_arm_gcc.h
*/
uint32 katomic_exchange(volatile uint32* ptr, uint32 new_value) {
	uint32 	old_value;
	int 	reloop;
	
	do {
		// old_value = LDREX(ptr)
		// reloop = STREX(ptr, new_value)
		__asm__ __volatile__("   ldrex %0, [%3]\n"
							 "   strex %1, %4, [%3]\n"
							 : "=&r"(old_value), "=&r"(reloop), "+m"(*ptr)
							 : "r"(ptr), "r"(new_value)
							 : "cc", "memory");
	} while (reloop != 0);
	
	return old_value;
}