#include <corelib/core.h>
#include <corelib/rb.h>
#include <main.h>

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
	
	/* packet loop */
	while (1) {
		/* read any packets in our buffer */
		sz = sizeof(pkt);
		while (er_read_nbio(&__corelib_rx, &pkt[0], &sz)) {
			switch (pkt[0]) {
				case KMSG_THREADMESSAGE:
					printf("[directory] got message %x\n", pkt[1]);
					break;
			}
			sz = sizeof(pkt);
		}
		/* wait for packets */
		sleep(0);
	}
	return 0;
}