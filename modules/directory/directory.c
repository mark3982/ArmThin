#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>
#include <corelib/linkhelper.h>
#include <corelib/vmessage.h>

static VMESSAGES			vmsgs;
static uint32				tps;


int main_pktarrived(void *arg, CORELIB_LINK *link) {
	VMESSAGE			*msg;
	void				*buf;
	uint32				sz;
	uint8				*buf8;
	uint32				x;
	uint32				avg;
	
	buf = malloc(link->rxesz);
	
	printf("[directory] packet arrived link:%x\n", link);
	
	sz = link->rxesz;
	while (lh_read_nbio(link, buf, &sz)) {
		printf("reading packet\n");
		/* let us see if it is a v-message */
		if (vmsg_checkread(&vmsgs, buf, link->rxesz, &msg)) {
			/* we have a v-message */
			printf("vmsg:%s\n", buf);
			
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