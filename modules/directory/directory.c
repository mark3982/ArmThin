#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>
#include <corelib/linkhelper.h>
#include <corelib/vmessage.h>

VMESSAGES			vmsgs;

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
	
	/* initialize v-messages structure */
	memset(&vmsgs, 0, sizeof(vmsgs));
	
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
	
	linkhelper_setoptarg(0);
	linkhelper_setpktarrived(&main_pktarrived);
	linkhelper_setlinkreq(&main_linkreq);
	linkhelper_setlinkdropped(&main_linkdropped);
	linkhelper_setlinkestablished(&main_linkestablished);
	linkhelper_setkmsg(&main_kmsg);
	
	while (1) {
		linkhelper_sleep(0);
		linkhelper_tick();
	}
	
	return 1;
}