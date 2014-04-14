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
	
	smmio = (unsigned int*)0xa0000000;
	
	printf("TESTUELF START\n");
	/* start sending packets */
	srand(0x98329);
	for (;;) {
		//printf("TESTUELF making packet\n");
		
		sz = ((uint32)rand() % 128) + 2;
		sz = 32 * 4;
		if (sz == 0) {
			continue;
		}
		//pkt[0] = ((uint32)rand() & 0x0f) | 0x80;
		//for (x = 1; x < sz; ++x) {
		//	pkt[x] = ((uint32)__rand(pkt[x - 1] & 0x0f) & 0x0f) | 0x80;
		//}
		
		pkt[0] = readtimer();
		//printf("pkt[0]:%x\n", pkt[0]);
		
		//printf("TESTUELF made packet..\n");

		/* sleep until we can write */
		while (rb_write_nbio(&__corelib_tx, &pkt[0], sz));
		/* keep waking kernel server to read messages */
		asm("swi #104");
		/* give up time slice */
		yield();
		//printf("TESTUELF wrote packet\n");
		/* alert kernel to new message in ring buffer */
	}
	
	for (;;);
	
	tps = getTicksPerSecond();
	
	for (;;) {
		printf("TESTUELF BEFORE SLEEP\n");
		sleep(1);
		printf("TESTUELF RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'B';
	}
	return 0;
}