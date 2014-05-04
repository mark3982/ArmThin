#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>

#define LDLCTIME		0xfffff

/*
	@sdescription:		Represents a uni/bi-directional IPC link.
*/
typedef struct _CORELIB_LINK {
	/* linked list members */
	struct _CORELIB_LINK		*next;
	struct _CORELIB_LINK		*prev;
	/* members */
	ERH				tx;
	ERH				rx;
	uintptr			addr;			/* address of first page of link */
	uintptr			pcnt;			/* number of pages comprising the link */
	uintptr			process;		/* remote process identifier */
	uintptr			thread;			/* remote thread identifier */
	uintptr			rsignal;		/* remote signal value */
	uintptr			lsignal;		/* local signal value */
} CORELIB_LINK;

typedef struct _CORELIB_LINKHELPER {
	/* @sdescription:		Root link to linked list of link structures. */
	CORELIB_LINK		*root;	
	/* @sdescription:		Maximum slots in array. Can be changed by allocating new array. */
	uint32				arraymax;
	/*
		@sdescription: 		Fast maps signal to corelib link.
		@ldescription:		The signal directly indexs into this array which
							holds a pointer to the actual link structure. This
							array is resized and copied as needed for expansion,
							but links always occupy the same signal slot.
	*/
	CORELIB_LINK		**array;
} CORELIB_LINKHELPER;


CORELIB_LINKHELPER			__corelib_linkhelper;

int server_handlepkt(CORELIB_LINK *link, uint32 *pkt, uint32 pktsz) {
	return 1;
}

int server_linkreq(uintptr rprocess, uintptr rthread) {
	return 1;	/* accept link request */
}

int server_init() {
	return 1;
}

uint32			linkhelper_ldlctime;
uint32			linkhelper_ldlc;

int linkhelper_init() {
	linkhelper_ldlc = 0;
	linkhelper_ldlctime = getTicksPerSecond() * 10;
	return 1;
}

int linkhelper_sleep() {
	sleep(linkhelper_ldlctime > (getosticks() - linkhelper_ldlc) ? (getosticks() - linkhelper_ldlc) : 1);
}
		
int linkhelper_tick() {
	uint32			pkt[32];
	uint32			sz;
	uintptr			tarproc;
	uintptr			tarsignal;
	uint32			ldlc;
	CORELIB_LINK	*link, *nlink;
	
	/* read any packets in our buffer */
	sz = sizeof(pkt);
	printf("[corelib] [linkhelper] checking for packets from kernel thread\n");
	while (er_read_nbio(&__corelib_rx, &pkt[0], &sz)) {
		printf("[corelib] [linkhelper] got pkt type:%x\n", pkt[0]);
		switch (pkt[0]) {
			case KMSG_THREADMESSAGE:
				printf("[corelib] [linkhelper] got message %x\n", pkt[1]);
				break;
			case KMSG_REQSHARED:
				printf("[corelib] [linkhelper] got REQSHARED\n");
				
				//app_kmsg_reqshared(
				
				pkt[0] = KMSG_ACPSHARED;
				pkt[6] = pkt[1];			/* set target RID */
				pkt[1] = 0x34;				/* set our RID */
				pkt[7] = 1;					/* set our signal */
				if (!er_write_nbio(&__corelib_tx, &pkt[0], sz)) {
					printf("[corelib] [linkhelper] write failed\n");
				}
				notifykserver();
				break;
			case KMSG_ACPSHAREDOK:
				printf("[directory] IPC connection established addr:%x\n", pkt[2]);
				/* wait for value */
				for (;;) {
					if (((uint32*)pkt[2])[0] == 0xdeedbeef) {
						printf("[directory] shared GOOD\n");
					}
				}
				break;
		}
		sz = sizeof(pkt);
	}
	
	/* read any signals and check corrosponding link */
	while (getsignal(&tarproc, &tarsignal)) {
		/* if too high just ignore it */
		if (tarsignal >= __corelib_linkhelper.arraymax) {
			continue;
		}
		
		link = __corelib_linkhelper.array[tarsignal];
		
		printf("[corelib] [linkhelper] reading link by signal %x (process:%x)\n", tarsignal, link->process);
		/* read all packets from link */
		sz = sizeof(pkt);
		while (er_read_nbio(&link->rx, &pkt[0], &sz)) {
			/* handle packet */
			server_handlepkt(link, &pkt[0], sz);
			sz = sizeof(pkt);
		}
	}
	
	if (getosticks() - linkhelper_ldlc > linkhelper_ldlctime) {
		linkhelper_ldlc = getosticks();
		printf("[corelib] [linkhelper] looking for dead links\n");
		/* checking for dead links .. one method to handle terminations */
		for (link = __corelib_linkhelper.root; link; link = nlink) {
			nlink = link->next;
			if (getvirtref(link->addr) < 2) {
				vfree(link->addr, link->pcnt);
				ll_rem((void**)&__corelib_linkhelper.root, link);
				printf("[corelib] [linkhelper] dropped link for process:%x addr:%x(%x)\n", link->process, link->addr, link->pcnt);
				free(link);
			}
		}
	}
	
	return 1;
}

int main() {
	uint32			pkt[32];
	uint32			sz;
	
	/* register as directory service */
	sz = sizeof(pkt);
	pkt[0] = KMSG_REGSERVICE;
	pkt[1] = 0x11223344;
	pkt[2] = KSERVICE_DIRECTORY;
	pkt[3] = 0;						/* filled with our process ID */
	pkt[4] = 0;						/* filled with our thread ID */
	er_write_nbio(&__corelib_tx, &pkt[0], 5 * 4);
	notifykserver();
	
	linkhelper_init();
	
	while (1) {
		linkhelper_sleep();
		linkhelper_tick();
	}
	
	return 1;
}