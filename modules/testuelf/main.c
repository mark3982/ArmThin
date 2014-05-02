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

/* 
	@name:				Write And Wait On Request Reply
	@description:
						Will block until reply is recieved, and
						ignore all other messages except for
						the one it is looking for.
					
	@param:erg:			structure representing entry ring
	@param:out:			pointer to data being written
	@param:sz:			length of data in bytes
	@param:rid32ndx		32-bit word offset in reply to check RID
	@param:rid:			request ID to look for
	@param:timeout:		the amount of time to wait in seconds before returning
*/
int er_worr(ERH *rx, void *out, uint32 sz, uint32 rid32ndx, uint32 rid, uint32 timeout) {
	uint8			*mndx;
	uint32			rem;

	rem = timeout;
	
	/* wait for reply */
	while (rem > 0) {
		while (er_peek_nbio(rx, out, &sz, &mndx)) {
			/* is this what we are looking for? */
			printf("[testuelf] GOT PACKET pkt.rid:%x rid:%x\n", ((uint32*)out)[rid32ndx], rid);
			if (((uint32*)out)[rid32ndx] == rid) {
				/* deallocate entry */
				mndx[0] = 0;
				return 1;
			}
		}
		/* sleep for tick count not seconds */
		printf("[testuelf] sleeping while waiting for reply\n");
		rem = sleepticks(timeout);
	}
	return 0;
}

int er_waworr(ERH *tx, ERH *rx, void *out, uint32 sz, uint32 rid32ndx, uint32 rid, uint32 timeout) {
	uint8			*mndx;
	uint32			rem;
	
	/* try to write it */
	if (!er_write_nbio(tx, out, sz)) {
		return 0;
	}
	
	/* alert thread that a message has arrived */
	if (tx->rproc == 0 && tx->rthread == 0) {
		/* kthread uses a more efficent signal and wakeup system */
		asm volatile ("swi %[code]" : : [code]"i" (KSWI_KERNELMSG));
	} else {
		/* send a signal then wake up thread incase it is sleeping */
		signal(tx->rproc, tx->rthread, tx->signal);
		wakeup(tx->rproc, tx->rthread); 
	}
	
	rem = timeout;
	
	printf("[testuelf] waiting for reply\n");
	/* wait for reply */
	while (rem > 0) {
		while (er_peek_nbio(rx, out, &sz, &mndx)) {
			/* is this what we are looking for? */
			printf("[testuelf] GOT PACKET pkt.rid:%x rid:%x\n", ((uint32*)out)[rid32ndx], rid);
			if (((uint32*)out)[rid32ndx] == rid) {
				/* deallocate entry */
				mndx[0] = 0;
				return 1;
			}
		}
		/* sleep for tick count not seconds */
		printf("[testuelf] sleeping while waiting for reply\n");
		rem = sleepticks(timeout);
	}
	
	return 0;
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
	uintptr			dirproc;
	uintptr			dirthread;
	uintptr			dirsignal;
	
	smmio = (unsigned int*)0xa0000000;
	
	smmio[0] = '+';
	
	printf("TESTUELF START\n");
	
	tps = getTicksPerSecond();
	
	addr = valloc(1);
	printf("addr:%x\n", addr);
	((uint32*)addr)[0] = 0x994;
	printf("-----------------addr:%x\n", addr);
	//vfree(addr, 1);
	
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
	}
	
	printf("[testuelf] woke up\n");
	
	printf("[testuelf] thread start detected\n");
	
	/* wait for reply for service enum */
	while (1) {
		/* get directory service */
		sz = sizeof(pkt);
		pkt[0] = KMSG_ENUMSERVICE;
		pkt[1] = 0x11223344;
		pkt[2] = KSERVICE_DIRECTORY;
		
		printf("[testuelf] sleeping for enum req reply\n");
		er_waworr(&__corelib_tx, &__corelib_rx, &pkt[0], 5 * 4, 1, pkt[1], tps * 30);
		printf("[testuelf] looking for reply packet - pkt[0]:%x pkt[1]:%x\n", pkt[0], pkt[1]);
		if (pkt[0] == KMSG_ENUMSERVICEREPLY && pkt[3] != 0) {
			printf("[testuelf] got directory service at %x:%x\n", pkt[3], pkt[4]);
			/* establish connection to directory service */
			dirproc = pkt[3];
			dirthread = pkt[4];
			dirsignal = pkt[7];
			break;
		}
		
		printf("[testuelf] sleeping for end of cycle\n");
		sleep(5);
	}
	
	/* we have the directory service process:thread ID now try to open a connection */
	pkt[0] = KMSG_REQSHARED;
	pkt[1] = 0x12344321;
	pkt[2] = addr;
	pkt[3] = 1;
	pkt[4] = dirproc;
	pkt[5] = dirthread;
	pkt[6] = 1;
	pkt[7] = KPROTO_ENTRYRING;
	pkt[8] = 4096 >> 1;
	pkt[9] = 4096 >> 1;
	/* send and wait for response from kserver */
	er_waworr(&__corelib_tx, &__corelib_rx, &pkt[0], 10 * 4, 1, pkt[1], tps * 30);
	printf("[testuelf] got response from kserver type:%x\n", pkt[0]);
	
	/* listen for the acceptance/rejection message */
	if (!er_worr(&__corelib_rx, &pkt[0], sizeof(pkt), 1, 0x12344321, tps * 30)) {
		printf("[testuelf] timeout waiting for reply! HALTED\n");
		for (;;);
	}
	
	/* write to shared memory to trigger response */
	printf("writting to %x\n", pkt[2]);
	((uint32*)pkt[2])[0] = 0xdeedbeef;
	/* set signal for directory to notify it which IPC connection to read from */
	signal(dirproc, dirthread, dirsignal);	
	
	for (;;);
	return 0;
}