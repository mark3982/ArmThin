#ifndef CORELIB_H_VMESSAGE
#define CORELIB_H_VMESSAGE
#include "linkhelper.h"
/*
	once totcnt == gotcnt we have a complete message
*/
typedef struct _VMESSAGE {
	struct _VMESSAGE	*next;
	struct _VMESSAGE	*prev;
	uint8				*buf;				/* allocated buffer */
	uint32				id;					/* id of this buffer */
	uint32				size;				/* size of buffer */
	uint32				gotcnt;				/* number of parts got */
	uint32				totcnt;				/* total number of parts */
} VMESSAGE;

typedef struct _VMESSAGES {
	VMESSAGE			*fmsg;				/* first message in chain */
} VMESSAGES;

int vmsg_init(VMESSAGES *h);
int vmsg_disect(void *buf, uint16 *id, uint8 *total, uint8 *index);
int vmsg_write(CORELIB_LINK *link, void *buf, uint32 bufsz);
int vmsg_checkread(VMESSAGES *h, void *buf, uint32 esz, VMESSAGE **out);
int vmsg_readex(VMESSAGES *h, uint16 id, uint8 total, uint8 index, void *buf, uint32 esz, VMESSAGE **out);
int vmsg_discard(VMESSAGES *h, VMESSAGE *m);
#endif