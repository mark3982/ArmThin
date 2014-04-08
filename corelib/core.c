#include "core.h"

unsigned int getTicksPerSecond() {
	unsigned int			out;
	asm("	swi #103 \n\
			mov %[out], r0 \n\
		" : [out]"=r" (out));
	return out;
}

void sleep(unsigned int timeout) {
	asm("	mov r0, %[in] \n\
			swi #101 \n\
		" : : [in]"r" (timeout));
}