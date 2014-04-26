#include "corelib/core.h"
#include "corelib/rb.h"
#include "main.h"

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
	
	sz = sizeof(pkt);
	pkt[0] = KMSG_SENDMESSAGE;
	for (x = 1; x < 32; ++x) {
		pkt[x] = x; 
	}
	rb_write_nbio(&__corelib_tx, &pkt[0], sz);
	notifykserver();
	
	for (;;) {
		printf("TESTUELF BEFORE SLEEP\n");
		sleep(1);
		printf("TESTUELF RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'B';
		smmio[0] = '\n';
	}
	return 0;
}