#include <corelib/core.h>
#include <corelib/rb.h>
#include <main.h>

void testthread() {
	for (;;) {
		//printf("TEST THREAD\n");
	}
}

int main() {
	int				x;
	int				y;
	unsigned int	tps;
	unsigned int 	*smmio;
	uint32			pkt[32];
	uint32			sz;
	uintptr			addr;
	uint32			stack[32];
	
	smmio = (unsigned int*)0xa0000000;
	
	smmio[0] = '+';
	
	printf("TESTUELF START\n");
	
	tps = getTicksPerSecond();
	
	addr = valloc(1);
	printf("-----------------addr:%x\n", addr);
	vfree(addr, 1);
	
	printf("testthread:%x\n", &testthread);
	
	sz = sizeof(pkt);
	pkt[0] = KMSG_CREATETHREAD;
	pkt[1] = 0x11223344;
	pkt[2] = (uintptr)&testthread;
	pkt[3] = (uintptr)&stack[30];
	pkt[4] = 0x12AA34BB;
	//rb_write_nbio(&__corelib_tx, &pkt[0], sz);
	//notifykserver();
	
	for (;;) {
		printf("TESTUELF BEFORE SLEEP\n");
		sleep(1);
		printf("TESTUELF RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'B';
		smmio[0] = '\n';
	}
	return 0;
}