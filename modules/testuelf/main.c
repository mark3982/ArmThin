#include "corelib/core.h"

uint32 readtimer() {
	uint32			*mmio;
	
	mmio = (uint32*)0xb0000200;
	return mmio[1];
}

int main() {
	int				x;
	int				y;
	unsigned int	tps;
	unsigned int 	*smmio;
	uint32			pkt[32];
	uint32			sz;
	uintptr			addr;
	
	smmio = (unsigned int*)0xa0000000;
	
	smmio[0] = '+';
	
	printf("TESTUELF START\n");
	
	tps = getTicksPerSecond();
	
	addr = valloc(1);
	printf("-----------------addr:%x\n", addr);
	vfree(addr, 1);
	
	for (;;) {
		printf("TESTUELF BEFORE SLEEP\n");
		sleep(1);
		printf("TESTUELF RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'B';
	}
	return 0;
}