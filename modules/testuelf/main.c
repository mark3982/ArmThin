#include "corelib/core.h"

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
	for (;;) {
		printf("TESTUELF making packet\n");
		sz = (uint32)rand() & 0xf;
		pkt[0] = (uint32)rand();
		srand(pkt[0]);
		for (x = 1; x < sz; ++x) {
			pkt[x] = (uint32)rand();
		}
		
		printf("TESTUELF made packet..\n");
		
		/* sleep until we can write */
		while (!rb_write_nbio(&__corelib_tx, &pkt[0], sz)) {
			sleep(1);
		}
		printf("TESTUELF wrote packet\n");
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