#include "stdtypes.h"
#include "main.h"
#include "atomic.h"

uint32 katomic_locktake8(volatile uint8 *ptr, uint8 need_value, uint8 new_value) {
	uint32 volatile		reloop;
	uint32 volatile		old_value;
	
	reloop = 1;
	// old_value = LDREX(ptr)
	// reloop = STREX(ptr, new_value)
	asm volatile (
				 "   ldrexb %[old_value], %[ptr]\n"
				 "   cmp %[old_value], %[new_value]\n"
				 "   moveq %[reloop], #0x0\n"
				 "   beq done%=\n"
				 "   cmp %[need_value], %[old_value]\n"
				 "   strexbeq %[reloop], %[new_value], %[ptr]\n"
				 "   done%=:\n"
				 : [reloop]"=r"(reloop), [ptr]"+m"(*ptr), [old_value]"=r" (old_value)
				 : [new_value]"r"(new_value), [need_value]"r"(need_value)
				 : "cc", "memory");
	/* reloop IS zero if success */
	return reloop == 0;
}

/*
	non-re-entrant lock for user space
*/
int katomic_lockspin_yield8nr(volatile uint8 *ptr, uint8 id) {
	while (!katomic_locktake8(ptr, 0, id) || *ptr != id) {
		asm("swi %[code]" : : [code]"i" (KSWI_YEILD));
	}
}