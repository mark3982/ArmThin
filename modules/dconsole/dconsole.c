#include <main.h>
#include <modules/directory/directory.h>
#include <corelib/core.h>
#include <corelib/rb.h>
#include <corelib/linkhelper.h>
#include <corelib/vmessage.h>

static VMESSAGES			vmsgs;
static uint32				tps;
static uintptr				dirproc;
static uintptr				dirthread;

int main_pktarrived(void *arg, CORELIB_LINK *link) {
	VMESSAGE			*msg;
	void				*buf;
	uint32				sz;
	uint8				*buf8;
	uint32				x;
	
	buf = malloc(link->rxesz);
	
	printf("[dconsole] packet arrived link:%x\n", link);
	
	sz = link->rxesz;
	while (lh_read_nbio(link, buf, &sz)) {
		printf("reading packet\n");
		/* let us see if it is a v-message */
		if (vmsg_checkread(&vmsgs, buf, link->rxesz, &msg)) {
			/* we have a v-message */
			buf8 = (uint8*)msg->buf;			
			
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
	printf("[dconsole] link request process:%x thread:%x proto:%x\n", process, thread, proto);
	return 1;
}

int main_linkdropped(void *arg, CORELIB_LINK *link) {
	printf("[dconsole] link dropped link:%x\n", link);
	return 1;
}

int dir_createnode(CORELIB_LINK *link, uint16 iface, char *path) {
	uint32		pkt[10];
	uint8		*pkt8;
	uint32		x;
	
	pkt[0] = DIRECTORY_CREATENODE;
	pkt[1] = 1;
	pkt[2] = iface;
	/* account for null-terminator (part of size) */
	pkt[3] = strlen(path) + 1;
	printf("[dconsole] strlen(%s):%x\n", path, strlen(path));
	pkt8 = (uint8*)&pkt[4];
	for (x = 0; x < strlen(path); ++x) {
		pkt8[x] = path[x];
	}
	pkt8[4 + x] = 0;
	
	vmsg_write(link, pkt, 1 + 1 + 2 + strlen(path) + 1);
	return 1;
}

int main_linkestablished(void *arg, CORELIB_LINK *link) {
	uint32		pkt[15];

	printf("[dconsole] link established link:%x\n", link);
	
	/* create console buffer for link */
	if (link->process == dirproc && link->thread == dirthread) {
		/* create our node in the directory system */
		dir_createnode(link, DIRECTORY_IF_VCONSOLE, "/dev/vconsole");
		signal(link->process, link->thread, link->rsignal);
		switchto(link->process, link->thread);
	}
	
	return 1;
}

int main() {
	uint32			pkt[32];
	uint32			sz;
	
	printf("[dconsole] initialized\n");
	
	tps = getTicksPerSecond();
	
	/* initialize v-messages structure */
	vmsg_init(&vmsgs);
	
	lh_init();
	
	lh_setdbgname("dconsole");
	lh_setoptarg(0);
	lh_setpktarrived(&main_pktarrived);
	lh_setlinkreq(&main_linkreq);
	lh_setlinkdropped(&main_linkdropped);
	lh_setlinkestablished(&main_linkestablished);
	lh_setkmsg(&main_kmsg);
	
	sz = sizeof(pkt);
	pkt[0] = KMSG_ENUMSERVICE;
	pkt[1] = 0x11223344;
	pkt[2] = KSERVICE_DIRECTORY;
	
	while (1) {
		printf("[dconsole] sleeping for enum req reply\n");
		er_waworr(&__corelib_tx, &__corelib_rx, &pkt[0], 5 * 4, 1, pkt[1], tps * 30);
		printf("	looking for reply packet - pkt[0]:%x pkt[1]:%x\n", pkt[0], pkt[1]);
		if (pkt[0] == KMSG_ENUMSERVICEREPLY && pkt[3] != 0) {
			printf("	got directory service at %x:%x\n", pkt[3], pkt[4]);
			/* establish connection to directory service */
			dirproc = pkt[3];
			dirthread = pkt[4];
			break;
		}
	}
	
	/* request link to remote directory service */
	if (!lh_establishlink(dirproc, dirthread, IPC_PROTO_ER, 2048, 2048, 16 * 4, 16 * 4, 0x12345678)) {
		printf("[dconsole] establish link failed\n");
		return -1;
	}
	
	printf("[dconsole] going into main loop\n");
	
	while (1) {
		lh_tick();
		lh_sleep(0);
	}
	
	return 1;
}