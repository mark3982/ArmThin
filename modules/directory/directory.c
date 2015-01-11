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
	uint32				ifcnt;
	uint32				*ifs;
} SIMPLENODE;

static SIMPLENODE		*nodes;


int main_pktarrived(void *arg, CORELIB_LINK *link) {
	VMESSAGE			*msg;
	void				*buf;
	uint32				sz;
	uint8				*buf8;
	uint32				x, y;
	uint32				avg;
	uint16				ifcnt;
	SIMPLENODE			*node;
	uint32				rid;
	uint32				*buf32;
	
	buf = malloc(link->rxesz);
	
	printf("[directory] packet arrived link:%x\n", link);
	
	sz = link->rxesz;
	while (lh_read_nbio(link, buf, &sz)) {
		printf("reading packet\n");
		/* let us see if it is a v-message */
		if (vmsg_checkread(&vmsgs, buf, link->rxesz, &msg)) {
			/* we have a v-message */
			buf8 = (uint8*)msg->buf;
			buf32 = (uint32*)msg->buf;
			
			printf("[directory] got v-msg type:%x\n", buf8[0]);
			
			/* check what type of message */
			switch (buf32[0]) {
				case DIRECTORY_CREATENODE:
						printf("[directory] got CREATENODE packet\n");
						/*
							TODO: check for node already existing
						*/
						printf("[directory] adding node\n");
						node = (SIMPLENODE*)malloc(sizeof(SIMPLENODE));
						/* read interfaces */
						ifcnt = (uint16)buf32[1];
						node->ifcnt = ifcnt;
						node->ifs = (uint32*)malloc(sizeof(uint32) * ifcnt);
						for (x = 0;	x < ifcnt; ++x) {
							node->ifs[x] = buf32[2];
							printf("	iface:%x\n", node->ifs[x]);
						}
						
						if (buf32[x + 3] + (3 * 4) > msg->size) {
							/* resource name too large */
							printf("	res-name too large [%x]\n", buf32[x + 3]);
							break;
						}
						
						node->path = (uint8*)malloc(buf32[x + 3]);
						
						/* copy path */
						buf8 = (uint8*)&buf32[3 + ifcnt + 1];
						for (y = 0; y < buf32[x + 3]; ++y) {
							node->path[y] = buf8[x];
						}
						node->path[y - 1] = 0; 
						/* null-terminate path */
						printf("	path:%s\n", node->path); 
						/* create node */
						ll_add((void**)&nodes, node);
					break;
				case DIRECTORY_QUERYDIR:
					/* get directory information */
					break;
				case DIRECTORY_QUERYNODE:
					/* get node */
					/*
						send reply with node information
						
						process:		0x9823				(process managing node)
						thread:			0x9283				(thread managing node)
						interfaces:		0x3452, 0x8273		(interfaces node supports)
						
						the interfaces specify how to talk to
						the node which is hosted at process:thread
					*/
					
					rid = buf32[1];
					buf8 = (uint8*)&buf32[3];
					
					if (buf32[2] + 3 * 4 >= msg->size) {
						/* resource-name too large */
						break;
					}
					
					/* enforce null terminator as part of resource name */
					buf8[buf32[2] - 1] = 0;
					
					for (node = nodes; node; node = node->next) {
						if (strcmp(node->path, buf8) == 0) {
							/* found the node so now return it's info */
							buf32[0] = rid;
							buf32[1] = node->ifcnt;
							for (x = 0; x < node->ifcnt; ++x) {
								buf32[2 + x] = node->ifs[x];
							}
							break;
						}
					}
					
					if (!node) {
						buf32[0] = rid;
						buf32[1] = 0;
						break;
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