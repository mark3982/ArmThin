#include "vmessage.h"

// totcnt, curndx, 0x80000000 (flag), id
// 16 might need to be at least 8-bit (maybe check buffer before writing to find new unique id)
// [1:flag][15:id][8:totcnt][8:curndx]
// 64 byte * 256 = 16KB

int vmsg_init(VMESSAGES *h) {
	h->fmsg = 0;
}

/*
	@sdescription:			internal function used to copy data into v-message buffer.
	@param:m:				pointer to v-message structure
	@param:index:			index for this supplied buffer
	@param:esz:				entry size (actual size) since we will subtract 4 from it
	@param:buf:				pointer to 5th byte of actual buffer read (4 byte make header)
*/
static int __vmsg_pktwrite(VMESSAGE *m, uint32 index, uint32 esz, uint8 *buf) {
	uint32		x;
	
	/* the 4 byte header was already stripped off */
	esz -= 4;
	
	for (x = 0; x < esz; ++x) {
		m->buf[x + (index * esz)] = buf[x];
	}
	
	++m->gotcnt;
	
	if (m->gotcnt == m->totcnt) {
		return 1;
	}
	
	return 0;
}

/*
	@description:		Helper function for determining if its an vmessage packet,
						and will extract out (assuming) the standard header the parts.
*/
int vmsg_disect(void *buf, uint16 *id, uint8 *total, uint8 *index) {
	uint32		*_buf;
	
	_buf = (uint32*)buf;
	
	if (!(_buf[0] & 0x80000000)) {
		return 0;
	}
	
	*id = (_buf[0] >> 16) & 0x7fff;
	*total = (_buf[0] >> 8) & 0xff;
	*index = _buf[0] & 0xff;
	return 1;
}

/*
	@sdescription:		Will write a v-message using the standard linkhelper write function.
*/
int vmsg_write(CORELIB_LINK *link, void *buf, uint32 bufsz, uint32 esz) {
	uint32		pcnt;
	uint32		x, y;
	uint8		*lbuf;
	uint16		id;
	
	/* account for header */
	esz -= 4;
	/* calculate number of parts */
	pcnt = (bufsz / esz) * esz < bufsz ? (bufsz / esz) + 1: bufsz / esz;
	/* sanity check */
	if (pcnt > 0xff) {
		/* too big.. unless esz is made bigger */
		return -1;
	}
	
	/* find free id */
	id = 0;
	
	lbuf = (uint8*)malloc(esz);
	
	for (x = 0; x < pcnt; ++x) {
		((uint32*)lbuf)[0] = 0x80000000 | (id << 16) | (pcnt << 8) | x;
		for (y = 0; y < esz; ++y) {
			/* incoming buffer might be slightly smaller */
			if (x * esz + y > bufsz) {
				break;
			}
			/* transfer byte */
			lbuf[4 + y] = ((uint8*)buf)[x * esz + y]; 
		}
		/* write v-message part (including 4-byte header) */
		while (!lh_write_nbio(link, lbuf, esz + 4)) {
			/* well.. the link's TX is likely full.. 
			   .. maybe the remote end will eventually 
			   read something.. in the mean time
			*/
			yield();
		}
	}
	
	free(lbuf);
	return 1;
}

/*
	@sdescription:		Will first check if buffer contains a v-message THEN it will
						process the v-message and produce a pointer to the v-message
						if it is completed.
	@param:h:			book keeping structure
	@param:buf:			pointer to complete buffer of size 'esz'
	@param:esz:			the size of the buffer AND the size of the underlying protocol entry size
	@param:out:			pointer to pointer to VMESSAGE; if return is 1
*/
int vmsg_checkread(VMESSAGES *h, void *buf, uint32 esz, VMESSAGE **out) {
	uint16			id;
	uint8			total;
	uint8			index;
	
	if (vmsg_disect(buf, &id, &total, &index)) {
		/*
			process the message AND if completed return value set to signal so and
			out will be set to the completed v-message structure
		*/
		return vmsg_readex(h, id, total, index, (void*)((uintptr)buf + 4), esz, out);
	}
	/* the message was not a v-message */
	return 0;
}

int vmsg_discard(VMESSAGES *h, VMESSAGE *m) {
	free(m->buf);
	free(m);
	return 1;
}

/*
	@sdescription:		Will process a v-message packet and handle storing the buffer contents
						until the remaining messages have been written.

	@param:h:			book keeping structure.
	@param:id:			ID (pre-extracted)
	@param:buf:			pointer to buffer (after the 4-byte header)
	@param:esz:			the size of an entry from the underlying protocol; buffer should be esz-4
	@param:out:			pointer to pointer to VMESSAGE; set to zero if no message
*/
int vmsg_readex(VMESSAGES *h, uint16 id, uint8 total, uint8 index, void *buf, uint32 esz, VMESSAGE **out) {
	VMESSAGE		*m, *nm;
	
	*out = 0;
	
	/* buf is assumed to be the length of esz-4 or larger */
	
	/* see if we can find message buffer */
	for (m = h->fmsg; m; m = nm) {
		nm = m->next;
		
		if (m->id == id) {
			if (((index * esz) + esz) > m->size || m->totcnt != total) {
				/* oops.. just drop this */
				ll_rem((void**)&h->fmsg, m);
				free(m->buf);
				free(m);
				return -1;
			}
			
			if (__vmsg_pktwrite(m, index, esz, buf)) {
				ll_rem((void**)&h->fmsg, m);
				*out = m;
				return 1;
			}
			
			return 0;
		}
	}
	
	/* create new buffer */
	m = (VMESSAGE*)malloc(sizeof(VMESSAGE));
	m->size = total * (esz - 4);
	m->buf = malloc(m->size);
	m->id = id;
	m->gotcnt = 0;
	m->totcnt = total;
	ll_add((void**)&h->fmsg, m);
	
	if (__vmsg_pktwrite(m, index, esz, buf)) {
		ll_rem((void**)&h->fmsg, m);
		*out = m;
		return 1;
	}
	
	return 0;
}