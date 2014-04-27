#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>

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
		/* sleep for signals */
		printf("[directory] sleeping until woken\n");
		sleep(0);
	
		/* read any packets in our buffer */
		sz = sizeof(pkt);
		printf("[directory] checking for packets\n");
		while (er_read_nbio(&__corelib_rx, &pkt[0], &sz)) {
			printf("[directory] got pkt type:%x\n", pkt[0]);
			switch (pkt[0]) {
				case KMSG_THREADMESSAGE:
					printf("[directory] got message %x\n", pkt[1]);
					break;
				case KMSG_REQSHARED:
					printf("[directory] got REQSHARED\n");
					pkt[0] = KMSG_ACPSHARED;
					pkt[6] = pkt[1];			/* set target RID */
					pkt[1] = 0x34;				/* set our RID */
					pkt[7] = 1;					/* set our signal */
					if (!er_write_nbio(&__corelib_tx, &pkt[0], sz)) {
						printf("[directory] write failed\n");
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
	}
	return 0;
}