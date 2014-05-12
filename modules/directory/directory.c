#include "directory.h"
#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>
#include <corelib/linkhelper.h>
#include <corelib/vmessage.h>

static VMESSAGES			vmsgs;
static uint32				tps;

typedef struct _SIMPLENODE {
	struct _SIMPLENODE	*next;
	struct _SIMPLENODE	*prev;
	uint8				*path;
	uint16				ifscnt;
	uint16				*ifs;
} SIMPLENODE;

static SIMPLENODE		*nodes;


int main_pktarrived(void *arg, CORELIB_LINK *link) {
	VMESSAGE			*msg;
	void				*buf;
	uint32				sz;
	uint8				*buf8;
	uint32				x;
	uint32				avg;
	uint16				ifcnt;
	SIMPLENODE			*node;
	
	buf = malloc(link->rxesz);
	
	printf("[directory] packet arrived link:%x\n", link);
	
	sz = link->rxesz;
	while (lh_read_nbio(link, buf, &sz)) {
		printf("reading packet\n");
		/* let us see if it is a v-message */
		if (vmsg_checkread(&vmsgs, buf, link->rxesz, &msg)) {
			/* we have a v-message */
			buf8 = (uint8*)msg->buf;
			
			printf("[directory] got v-msg type:%x\n", buf8[0]);
			
			/* check what type of message */
			switch (buf8[0]) {
				case DIRECTORY_CREATENODE:
						/*
							TODO: check for node already existing
						*/
						printf("[directory] adding node\n");
						node = (SIMPLENODE*)malloc(sizeof(SIMPLENODE));
						ifcnt = buf8[1];
						node->ifs = (uint16*)malloc(sizeof(uint16) * ifcnt);
						for (x = 0;	x < ifcnt; ++x) {
							node->ifs[x] = (buf8[2 + x * 2 + 0] << 8) | buf8[2 + x * 2 + 1];
							printf("	iface:%x\n", node->ifs[x]);
						}
						for (x = 0; (x < msg->size) && (buf8[x + (ifcnt * 2) + 2] != 0); ++x);
						node->path = (uint8*)malloc(x + 1);
						for (x = 0; (x < msg->size) && (buf8[x + (ifcnt * 2) + 2] != 0); ++x) {
							node->path[x] = buf8[x + (ifcnt * 2) + 2];
						}
						node->path[x] = 0;
						printf("	path:%s\n", node->path); 
						ll_add((void**)&nodes, node);
					break;
				case DIRECTORY_QUERYDIR:
					/* get directory information */
					break;
				case DIRECTORY_QUERYNODE:
					/* get node */
					/*
						send reply with node information
						
						process:		0x9823
						thread:			0x9283
						interfaces:		0x3452, 0x8273
						
						the interfaces specify how to talk to
						the node which is hosted at process:thread
					*/
					for (x = 1; (x < msg->size) && (msg->buf[x] != 0); ++x);
					
					for (node = nodes; node; node = node->next) {
						if (strcmp(node->path, &msg->buf[1]) == 0) {
							/* found the node so now return it's info */
							
						}
					}
					break;
				default:
					break;
			}
			
			
			/* we are done processing message (free resources) */
			vmsg_discard(&vmsgs, msg);
		}

		
		sz = link->rxesz;
	}
	
	return 1;
}

int main_kmsg(void *arg, uint32 *pkt, uint32 sz) {
}

int main_linkreq(void *arg, uintptr process, uintptr thread, uint32 proto) {
	printf("[directory] link request process:%x thread:%x proto:%x\n", process, thread, proto);
	return 1;
}

int main_linkdropped(void *arg, CORELIB_LINK *link) {
	printf("[directory] link dropped link:%x\n", link);
	return 1;
}

int main_linkestablished(void *arg, CORELIB_LINK *link) {
	printf("[directory] link established link:%x\n", link);
	return 1;
}

int main() {
	uint32			pkt[32];
	uint32			sz;
	
	printf("[directory] initialized\n");
	
	tps = getTicksPerSecond();
	
	/* initialize v-messages structure */
	vmsg_init(&vmsgs);
	
	/* register as directory service */
	sz = sizeof(pkt);
	pkt[0] = KMSG_REGSERVICE;
	pkt[1] = 0x11223344;
	pkt[2] = KSERVICE_DIRECTORY;
	pkt[3] = 0;						/* filled with our process ID */
	pkt[4] = 0;						/* filled with our thread ID */
	er_write_nbio(&__corelib_tx, &pkt[0], 5 * 4);
	notifykserver();
	
	printf("[directory] send registration for directory service to kernel\n");
	
	lh_init();
	
	lh_setdbgname("directory");
	lh_setoptarg(0);
	lh_setpktarrived(&main_pktarrived);
	lh_setlinkreq(&main_linkreq);
	lh_setlinkdropped(&main_linkdropped);
	lh_setlinkestablished(&main_linkestablished);
	lh_setkmsg(&main_kmsg);
	
	while (1) {
		lh_sleep(0);
		lh_tick();
	}
	
	return 1;
}