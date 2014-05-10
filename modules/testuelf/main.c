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
		notifykserver();
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

/*
	@sdescription:			Requests a link to a remote process and thread through the kernel. It will
							not wait for the reply, but will rather catch it during the tick.
	@param:rid:				request id
	@param:rprocess:		remote process
	@param:rthread:			remote thread
	@param:proto:			protocol
	@param:lsignal:			local signal to be used by remote
	@param:addr:			address of memory chunk to be used
	@param:pcnt:			page count of memory chunk
	@param:rxsz:			size of RX		(remote RX)
	@param:txsz:			size of TX		(remote TX)
	@param:rxesz:			(if entry based) entry size
	@param:txesz:			(if entry based) entry size
*/

int lh_establishlink(uint32 rprocess, uint32 rthread, uint32 proto, uint32 rxsz, uint32 txsz, uint32 rxesz, uint32 txesz, uint32 rid) {
	uint32		pkt[12];
	uint32		lsignal;
	uint32		addr;
	uint32		psize;
	uint32		pcnt;
	int			res;
	
	
	printf("getting lsignal\n");
	/* come back and fix this */
	lsignal = lh_getnewsignalid();
	printf("got lsignal\n");
	
	/* get total memory needed then round up to nearest whole page */
	pcnt = rxsz + txsz;
	psize = getpagesize();
	pcnt = (pcnt / psize) * psize < pcnt ? (pcnt / psize) + 1 : pcnt / psize;
	/* allocate memory */
	addr = valloc(pcnt);
	
	/* build the packet */
	pkt[0] = KMSG_REQSHARED;
	pkt[1] = rid;					/* request id */
	pkt[2] = addr;					/* address of memory */
	pkt[3] = pcnt;					/* page count */
	pkt[4] = rprocess;				/* target process */
	pkt[5] = rthread; 				/* target thread */
	pkt[6] = lsignal;				/* signal we want used */
	pkt[7] = proto;					/* protocol to be used */
	pkt[8] = rxsz;					/* size of rx */
	pkt[9] = txsz;					/* size of tx */
	pkt[10] = rxesz;				/* size of rx entry */	
	pkt[11] = txesz;				/* size of tx entry */

	/* kernel message */
	res = er_write_nbio(&__corelib_tx, &pkt[0], sizeof(pkt));
	if (!res) {
		return res;
	}
	
	/* notify kernel of message */
	notifykserver();
	
	return res;
	
	//er_waworr(&__corelib_tx, &__corelib_rx, &pkt[0], 10 * 4, 1, pkt[1], 0);
	//printf("[lh] got response from kserver type:%x\n", pkt[0]);
	
	/* listen for the acceptance/rejection message */
	//if (!er_worr(&__corelib_rx, &pkt[0], sizeof(pkt), 1, 0x12344321, 0)) {
	//	printf("[lh] timeout waiting for reply! HALTED\n");
	//	return 0;
	//}
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

	printf("[app] link established link:%x\n", link);
	//int lh_write_nbio(CORELIB_LINK *link, void *p, uint32 sz);
	printf("[testuelf] SENDING HELLO\n");
	
	pkt[0] = 0xaa;
	pkt[1] = 0xbb;
	pkt[2] = 0xcc;
	pkt[3] = 0xdd;
	for (;;) {
		if (!lh_write_nbio(link, pkt, sizeof(pkt))) {
			signal(link->process, link->thread, link->rsignal);
			wakeup(link->process, link->thread);
			switchto(link->process, link->thread);
			yield();
		}
		
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
	if (!lh_establishlink(dirproc, dirthread, IPC_PROTO_ER, 4096 >> 1, 4096 >> 1, 16 * 4, 16 * 4, 0x12345678)) {
		printf("[app] establish link failed\n");
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