#include <corelib/linkhelper.h>

/* 
	This just places a layer
	between the application containing a lot of code that would be duplicated
	by most applications/servers/services. It helps handle the complexity of
	managing multiple IPC links, and will hopefully provide utility functions
	for sending common messages to the kernel for IPC management.
	
	It supports multiple protocols and tries to setup the link for the specific
	protocol if supported. I need to add a callback for allowing the application
	to setup protocol not support by default so the generic functions can be
	used such as lh_write_nbio, lh_read_nbio, and lh_peek_nbio for those protocols
	not defaultly supported.
	
	Also, more functions need to be added for various inspection of the protocol such
	as maximum message size to basically layer away the application from needing to
	know the exact protocol inside and out just to use it although for some protocols
	this may be impossible, but hopefully fail out behavior will be possible so at
	least the application can detect failure and alert the user or log it.
*/

/*
	@sdescription:		Generalized IPC link write operation.
*/
int lh_write_nbio(CORELIB_LINK *link, void *p, uint32 sz) {
	int		ret;

	if (link->wnbio) {
		return link->wnbio(link->tx, p, sz);
	}
	
	return -1;
}

/*
	@sdescription:		Generalized IPC link read operation.
*/
int lh_read_nbio(CORELIB_LINK *link, void *p, uint32 *sz) {
	if (link->rnbio) {
		return link->rnbio(link->rx, p, sz);
	}
	
	return -1;
}

/*
	@sdescription:		Generalized IPC link peek operation.
*/
int lh_peek_nbio(CORELIB_LINK *link, void *p, uint32 *sz, uint8 **mndx) {
	if (link->pnbio) {
		return link->pnbio(link->rx, p, sz, mndx);
	}
	
	/* return failure, but non-supported code */
	return -1;
}

static CORELIB_LINKHELPER			glh;
static uint32						gldlctime;
static uint32						gldlc;

/*
	@sdescription:		Called when a kernel message arrives that is NOT handled.
*/
void lh_setkmsg(LH_KMSG h) {
	glh.handler_kmsg = h;
}

/*
	@sdescription:		Called when a packet arrives.
*/
void lh_setpktarrived(LH_PKTARRIVED h) {
	glh.handler_pktarrived = h;
}

/*
	@sdescription:		Called when IPC link is requested.
*/
void lh_setlinkreq(LH_LINKREQ h) {
	glh.handler_linkreq = h;
}

void lh_setlinkfailed(LH_LINKFAILED h) {
	glh.handler_linkfailed = h;
}

/*
	@sdescription:		Called when an IPC link is dropped.
*/
void lh_setlinkdropped(LH_LINKDROPPED h) {
	glh.handler_linkdropped = h;
}

/*
	@sdescription:		Called when an IPC link is established.
*/
void lh_setlinkestablished(LH_LINKESTABLISHED h) {
	glh.handler_linkestablished = h;
}

/*
	@sdescription:		Sets optional argument for the callbacks.
*/
void lh_setoptarg(void *arg) {
	glh.handler_arg = arg;
}

void lh_setdbgname(char *dbgname) {
	glh.dbgname = dbgname;
}

/*
	@sdescription:		Initializes the link helper system.
*/
int lh_init() {
	gldlc = 0;
	gldlctime = getTicksPerSecond() * 10;
	
	glh.dbgname = "unknown";
	
	memset(&glh, 0, sizeof(glh));
	
	/* initial signal array and max size */
	glh.arraymax = 100;
	glh.array = (CORELIB_LINK**)malloc(sizeof(CORELIB_LINK*) * glh.arraymax);
	memset(glh.array, 0, sizeof(CORELIB_LINK*) * glh.arraymax);
	return 1;
}

/*
	@sdescription:		If application is done, this will sleep at most the timeout specified unless its zero.
*/
int lh_sleep(uint32 timeout) {
	uint32		_timeout;
	uint32		osticks;
	
	osticks = getosticks();
	
	printf("osticks:%x gldlctime:%x\n", osticks, gldlctime);
	
	_timeout = gldlctime > (osticks - gldlc) ? gldlctime - (osticks - gldlc) : 1;
	if (timeout != 0 && timeout < _timeout) {
		_timeout = timeout;
	}
	printf("[corelib] [linkhelper] sleep for %x\n", _timeout);
	sleepticks(_timeout);
}

uint32 lh_getnewsignalid() {
	uint32		x;
	void		*tmp;
	
	printf("CCCC max:%x array:%x\n", glh.arraymax, glh.array);
	for (x = 10; x < glh.arraymax; ++x) {
		if (!glh.array[x]) {
			break;
		}
	}
	printf("AAAA x:%x\n", x);
	if (x >= glh.arraymax) {
		/*
			The array is not big enough. We need to reallocate it, and
			then also set 'x' to a free slot index.
		
			(1) save old pointer
			(2) alloc array at double current size
			(3) clear array to zeros
			(4) copy old array on top
			(5) set 'x' to next free slot
			(6) increase arraymax by *2
			(7) free old array
		*/
		tmp = glh.array;
		glh.array = (CORELIB_LINK**)malloc(sizeof(CORELIB_LINK*) * glh.arraymax * 2);
		memset(glh.array, 0, sizeof(CORELIB_LINK*) * glh.arraymax * 2);
		memcpy(glh.array, tmp, sizeof(CORELIB_LINK*) * glh.arraymax);
		x = glh.arraymax;
		glh.arraymax = glh.arraymax * 2;
		free(tmp);
	}
	
	printf("[%s] [corelib] [lh_getnewsignalid=%x]\n", glh.dbgname, x);
	
	return x;
}
	
/*
	@sdescription:		Performs various operations needed to maintain state.
*/	
int lh_tick() {
	uint32				pkt[32];
	uint32				sz;
	uintptr				tarproc;
	uintptr				tarsignal;
	uint32				ldlc;
	CORELIB_LINK		*link, *nlink;
	uint32				x;
	void				*tmp;
	
	/* read any packets in our buffer */
	sz = sizeof(pkt);
	printf("[%s] [corelib] [linkhelper] checking for packets from kernel thread\n", glh.dbgname);
	while (er_read_nbio(&__corelib_rx, &pkt[0], &sz)) {
		printf("[%s] [corelib] [linkhelper] got pkt type:%x\n", glh.dbgname, pkt[0]);
		switch (pkt[0]) {
			case KMSG_REQSHAREDOK:
				printf("[%s] [corelib] [lh] got KMSG_REQSHAREDOK\n", glh.dbgname);
				break;
			case KMSG_REQSHAREDFAIL:
				printf("[%s] [corelib] [linkhelper] got KMSG_REQSHAREDFAIL\n", glh.dbgname);
				glh.handler_linkfailed(glh.handler_arg, pkt[1]);
				break;
			case KMSG_REQSHARED:
				printf("[%s] [corelib] [linkhelper] got REQSHARED\n", glh.dbgname);
				
				if (glh.handler_linkreq) {
					if (!glh.handler_linkreq(glh.handler_arg, pkt[4], pkt[5], pkt[7])) {
						/* TODO: need support in kernel service to reject request */
						break;
					}
				}
				
				/*
					REQUEST FORMAT
					[0] - type
					[1] - requester RID
					[2] - memory offset
					[3] - page count
					[4] - requestor process id
					[5] - requester thread id
					[6] - requester specified signal (the signal to use for remote)
					[7] - protocol expected
					[8] - tx size
					[9] - rx size
					[10] - tx entry size
					[11] - rx entry size
				*/
				
				
				x = lh_getnewsignalid();
								
				pkt[0] = KMSG_ACPSHARED;
				pkt[12] = pkt[11];							/* save RX entry size */
				pkt[11] = pkt[10];							/* save TX entry size */
				pkt[10] = pkt[9];							/* save RX size */
				pkt[9] = pkt[8];							/* save TX size */
				pkt[8] = pkt[6];							/* save requester specified signal */
				pkt[6] = pkt[1];							/* set target RID */
				pkt[1] = 0x34;								/* set our RID */
				pkt[13] = pkt[7];
				pkt[7] = x;									/* set our signal (what signal remote uses) */
				
				if (!er_write_nbio(&__corelib_tx, &pkt[0], sz)) {
					printf("[%s] [corelib] [linkhelper] write failed\n", glh.dbgname);
				}
				notifykserver();
				break;
			case KMSG_ACPSHAREDREQUESTOR:
			case KMSG_ACPSHAREDACCEPTOR:
				printf("[%s] [corelib] [lh] IPC connection established addr:%x\n", glh.dbgname, pkt[2]);
				
				/*
					[0] - packet type
					[2] - address (virtual)
					[3] - page count
					
					[7] - the signal we choose for this
					[8] - the signal the remote end choose
					
					[9] - (saved) rx size
					[10] - (saved) tx size
					[11] - (saved) rx entry size
					[12] - (saved) tx entry size
				*/
				
				/* @[LINK-ESTABLISHMENT] */
				link = (CORELIB_LINK*)malloc(sizeof(CORELIB_LINK));	
	
				if (pkt[0] == KMSG_ACPSHAREDREQUESTOR) {
					link->rsignal = pkt[7];
					link->lsignal = pkt[8];
					link->rxsize = pkt[10];
					link->txsize = pkt[9];					
				} else {
					link->rsignal = pkt[8];
					link->lsignal = pkt[7];
					link->rxsize = pkt[9];
					link->txsize = pkt[10];
				}
				link->addr = pkt[2];
				link->pcnt = pkt[3];
				link->process = pkt[4];
				link->thread = pkt[5];
				
				printf("[%s] link->rsignal:%x link->lsignal:%x link->addr:%x link->pcnt:%x\n",
					glh.dbgname,
					link->rsignal, link->lsignal, link->addr, link->pcnt
				);
				
				printf("[%s] link->process:%x link->thread:%x link->rxsize:%x link->txsize:%x\n",
					glh.dbgname,
					link->process, link->thread, link->rxsize, link->txsize
				);
				
				glh.array[link->lsignal] = link;

	
				/* not entry based (variable) */
				link->rxesz = 0;
				link->txesz = 0;
	
				for (x = 0; x < 12; ++x) {
					printf("%x:%x\n", x, pkt[x]);
				}
				
	
				switch (pkt[13]) {
					case IPC_PROTO_RB:
						printf("[%s] [corelib] [lh] protocol set as IPC_PROTO_RB\n", glh.dbgname);
						link->wnbio = (LH_WRITE_NBIO)rb_write_nbio;
						link->rnbio = (LH_READ_NBIO)rb_read_nbio;
						link->pnbio = 0;					/* not supported */
						link->rx = malloc(sizeof(RBM));
						link->tx = malloc(sizeof(RBM));
						
						/* we have to swap the RX and TX for requestor */
						if (pkt[0] == KMSG_ACPSHAREDREQUESTOR) {
							rb_ready((RBM*)link->rx, (void*)(link->addr + link->rxsize), link->rxsize);
							rb_ready((RBM*)link->tx, (void*)link->addr, link->txsize);
						} else {
							rb_ready((RBM*)link->rx, (void*)link->addr, link->rxsize);
							rb_ready((RBM*)link->tx, (void*)(link->addr + link->rxsize), link->txsize);
						}
						break;
					case IPC_PROTO_ER:
						/* entry based non-variable */
						printf("[%s] [corelib] [lh] protocol set as IPC_PROTO_ER\n", glh.dbgname);
						link->rx = malloc(sizeof(ERH));
						link->tx = malloc(sizeof(ERH));
						
						link->rxesz = pkt[11];
						link->txesz = pkt[12];
						link->wnbio = (LH_WRITE_NBIO)er_write_nbio;
						link->rnbio = (LH_READ_NBIO)er_read_nbio;
						link->pnbio = (LH_PEEK_NBIO)er_peek_nbio;
						/*
							setup; passing size and entry size and also
							locking function for transmit buffer for multiple
							writers; lock can be optional (or it will fail if
							required)
						*/
						if (pkt[0] == KMSG_ACPSHAREDREQUESTOR) {
							er_ready(link->rx, (void*)(link->addr + link->rxsize), link->rxsize,  link->rxesz, 0);
							er_ready(link->tx, (void*)link->addr, link->txsize, link->txesz, &katomic_lockspin_yield8nr);
						} else {
							er_ready(link->rx, (void*)link->addr, link->rxsize,  link->rxesz, 0);
							er_ready(link->tx, (void*)(link->addr + link->rxsize), link->txsize, link->txesz, &katomic_lockspin_yield8nr);
						}
						break;
					default:
						/* unsupported protocol type (link has to be configured manually) */
						link->wnbio = 0;
						link->rnbio = 0;
						link->pnbio = 0;
						printf("[%s] [corelib] [linkhelper] UNSUPPORTED PROTOCOL:%x\n", glh.dbgname, pkt[13]);
						/* TODO: add better handling (such as user callback) */
						break;
				}
				
				glh.handler_linkestablished(
					glh.handler_arg,
					link
				);
				break;
			default:
				glh.handler_kmsg(glh.handler_arg, pkt, sz);
				break;
		}
		printf("[%s] [corelib] [linkhelper] ... loop\n", glh.dbgname);
		sz = sizeof(pkt);
	}
	printf("[%s] [corelib] [linkhelper] reading any signals\n", glh.dbgname);
	/* read any signals and check corrosponding link */
	while (getsignal(&tarproc, &tarsignal)) {
		printf("[%s] [corelib] [linkhelper] got signal %x from process %x\n", glh.dbgname, tarsignal, tarproc);
		/* if too high just ignore it */
		if (tarsignal >= glh.arraymax) {
			printf("[%s] [corelib] [linkhelper] tarsignal:%x > arraymax:%x\n",
				glh.dbgname, tarsignal, glh.arraymax
			);
			continue;
		}
		
		link = glh.array[tarsignal];
		
		if (!link) {
			printf("[%s] [corelib] [linkhelper] link invalid for signal\n", glh.dbgname);
			continue;
		}
		
		printf("[%s] [corelib] [linkhelper] reading link by signal %x (process:%x)\n", glh.dbgname, tarsignal, link->process);
		/* read all packets from link */
		//sz = sizeof(pkt);
		/*
			we are using the protocol specified instead of ER 
			here.. so now we respect and use the protocol established
			during establishment of this link
			
			@see:LINK-ESTABLISHMENT;
		*/
		//while (er_read_nbio(&link->rx, &pkt[0], &sz)) {
		//while (lh_read_nbio(link, &pkt[0], &sz)) {
			/* handle packet */
		//	server_handlepkt(link, &pkt[0], sz);
		//	sz = sizeof(pkt);
		//}
		if (glh.handler_pktarrived) {
			glh.handler_pktarrived(
				glh.handler_arg,
				link
			);
		}
	}
	
	printf("[%s] [corelib] [linkhelper] thinking about dropping dead links.. passed:%x expire:%x last:%x\n",
		glh.dbgname,
		getosticks() - gldlc,
		gldlctime,
		gldlc
	);
	if (getosticks() - gldlc > gldlctime) {
		gldlc = getosticks();
		printf("gldlc:%x\n", gldlc);
		printf("[%s] [corelib] [linkhelper] looking for dead links glh.root:%x\n", glh.dbgname, glh.root);
		/* checking for dead links .. one method to handle terminations */
		for (link = glh.root; link; link = nlink) {
			nlink = link->next;
			printf("	calling getvirtref\n");
			if (getvirtref(link->addr) < 2) {
				printf("	dropping link\n");
				if (glh.handler_linkdropped) {
					if (!glh.handler_linkdropped(
							glh.handler_arg,
							link
					)) {
						/* drop denied */
						continue;
					}
				}
				vfree(link->addr, link->pcnt);
				ll_rem((void**)&glh.root, link);
				printf("[%s] [corelib] [linkhelper] dropped link for process:%x addr:%x(%x)\n", glh.dbgname, link->process, link->addr, link->pcnt);
				free(link);
			}
		}
	}
	
	return 1;
}
