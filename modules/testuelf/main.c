#include <corelib/core.h>
#include <corelib/rb.h>
#include <corelib/linkhelper.h>
#include <corelib/vmessage.h>
#include <main.h>

static uint32		threadstarted;

VMESSAGES		vmsgs;


void msghandler() {
	for (;;) {
		printf("TEST THREAD ------- TEST THREAD\n");
		threadstarted = 1;
		sleep(5);
	}
}

int main_pktarrived(void *arg, CORELIB_LINK *link) {
	VMESSAGE			*msg;
	void				*buf;
	uint32				sz;
	
	buf = malloc(link->rxesz);
	
	printf("[directory] packet arrived link:%x\n", link);
	
	sz = link->rxesz;
	if (lh_read_nbio(link, buf, &sz)) {
		/* let us see if it is a v-message */
		if (vmsg_checkread(&vmsgs, buf, link->rxesz, &msg)) {
			/* we have a v-message */
			
			/* we are done processing message (free resources) */
			vmsg_discard(&vmsgs, msg);
		}
	}
	
	free(buf);
	
	return 1;
}

/* ------------------------------------------------------------------ */

int main_kmsg(void *arg, uint32 *pkt, uint32 sz) {
}

int main_linkreq(void *arg, uintptr process, uintptr thread, uint32 proto) {
	printf("[testuelf] link request process:%x thread:%x proto:%x\n", process, thread, proto);
	return 1;
}

int main_linkdropped(void *arg, CORELIB_LINK *link) {
	printf("[testuelf] link dropped link:%x\n", link);
	return 1;
}

int main_linkfailed(void *arg, uint32 rid) {
	printf("[testuelf] link failed rid:%x\n", rid);
	return 1;
}

int main_linkestablished(void *arg, CORELIB_LINK *link) {
	uint8		pkt[16 * 4];

	printf("[testuelf] link established link:%x\n", link);
	//int lh_write_nbio(CORELIB_LINK *link, void *p, uint32 sz);
	printf("[testuelf] SENDING HELLO\n");
	
	pkt[0] = 0xaa;
	pkt[1] = 0xbb;
	pkt[2] = 0xcc;
	pkt[3] = 0xdd;
	for (;;) {
		//if (!lh_write_nbio(link, pkt, sizeof(pkt))) {
		//	signal(link->process, link->thread, link->rsignal);
			//wakeup(link->process, link->thread);
		//	switchto(link->process, link->thread);
		//}
		
		printf("sending vmsg\n");
		vmsg_write(link, "hello world", 12);
		signal(link->process, link->thread, link->rsignal);
		switchto(link->process, link->thread);
		sleep(5);
		
	}
	return 1;
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

	printf("[testuelf] initialized\n");
	
	lh_init();
	lh_setdbgname("testuelf");
	lh_setoptarg(0);
	lh_setpktarrived(&main_pktarrived);
	lh_setlinkreq(&main_linkreq);
	lh_setlinkdropped(&main_linkdropped);
	lh_setlinkestablished(&main_linkestablished);
	lh_setkmsg(&main_kmsg);
	
	smmio = (unsigned int*)0xa0000000;
	
	vmsg_init(&vmsgs);
	
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
			break;
		}
		
		printf("[testuelf] sleeping for end of cycle\n");
		sleep(5);
	}
	
	/* request link to remote directory service */
	if (!lh_establishlink(dirproc, dirthread, IPC_PROTO_ER, 2048, 2048, 16 * 4, 16 * 4, 0x12345678)) {
		printf("[testuelf] establish link failed\n");
		return -1;
	}
	
	//dirsignal = pkt[7];
	
	/* write to shared memory to trigger response */
	//printf("writting to %x\n", pkt[2]);
	//((uint32*)pkt[2])[0] = 0xdeedbeef;
	
	
	/* set signal for directory to notify it which IPC connection to read from */
	//signal(dirproc, dirthread, dirsignal);	
	
	while (1) {
		/* we can do anything we need to do in here and adjust lh_sleep */
		lh_sleep(0);
		lh_tick();
	}
	
	for (;;);
	return 0;
}