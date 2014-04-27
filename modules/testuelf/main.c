#include <corelib/core.h>
#include <corelib/rb.h>
#include <main.h>

static uint32		threadstarted;

void msghandler() {
	for (;;) {
		printf("TEST THREAD ------- TEST THREAD\n");
		threadstarted = 1;
		sleep(5);
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
	
	threadstarted = 0;
	
	/* test creating another thread */
	sz = sizeof(pkt);
	pkt[0] = KMSG_CREATETHREAD;
	pkt[1] = 0x11223344;
	pkt[2] = (uintptr)&msghandler;
	pkt[3] = (uintptr)&stack[30];
	pkt[4] = 0x12AA34BB;
	er_write_nbio(&__corelib_tx, &pkt[0], 5 * 4);
	notifykserver();
	
	printf("[testuelf] waiting on thread to start\n");
	
	/* wait for thread */
	while (!threadstarted) {
		yield();
		printf("[testuelf] woke up\n");
	}
	
	printf("[testuelf] thread start detected\n");
	
	/* wait for reply for service enum */
	while (1) {
		/* get directory service */
		sz = sizeof(pkt);
		pkt[0] = KMSG_ENUMSERVICE;
		pkt[1] = 0x11223344;
		pkt[2] = KSERVICE_DIRECTORY;
		er_write_nbio(&__corelib_tx, &pkt[0], 5 * 4);
		notifykserver();
		printf("[testuelf] sleeping for enum req reply\n");
		sleep(0);
		printf("[testuelf] looking for reply packet\n");
		sz = sizeof(pkt);
		while (er_read_nbio(&__corelib_rx, &pkt[0], &sz)) {
			if (pkt[0] == KMSG_ENUMSERVICEREPLY) {
				printf("[testuelf] got directory service at %x:%x\n", pkt[3], pkt[4]);
				/* establish connection to directory service */
			}
		}
		
		printf("[testuelf] sleeping for end of cycle\n");
		sleep(5);
	}
	
	//KMSG_ENUMSERVICEREPLY
	
	for (;;);
	return 0;
}