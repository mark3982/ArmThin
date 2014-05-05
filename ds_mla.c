/*
	@group:			DataStructures.MultiLimitedArray
	@sdescription:	Provides a limited array per domain.
	@ldescription:	Provides a limited array per domain,
					of CPU word sized values.
	@author:		<Leonard Kevin McGuire Jr(kmcg3413@gmail.com)
*/
#include "ds_mla.h"

/*
	@group:			MultiLimitedArray
	@sdescription:	Initializes the data structure
	@param:sb:		object to be initialized
	@param:dmax:	default max (limited) slots per array (multi)
*/
int mla_init(MLA *sb, uint32 dmax) {
	sb->blocks = 0;
	sb->dmax = dmax;
}

/*
	@sdescription:		Will return the domain and value of one item.
	@param:sb:			pointer to object
	@param:+domain:		pointer to domain to be set
	@param:+out:		pointer to value to be set
*/
int mla_get(MLA *sb, uintptr *domain, uintptr *out) {
	MLAB			*mlab;
	uint32		x;

	kprintf("[mla_get] enter\n");
	for (mlab = sb->blocks; mlab; mlab = mlab->next) {
		if (mlab->used > 0) {
			kprintf("[mla_get] mlab->used:%x > 0\n", mlab->used);
			for (x = 0; x < mlab->max; ++x) {
				if (mlab->slots[x]) {
					kprintf("[mla_get] mlab->slots[%x]:%x != 0\n", x, mlab->slots[x]);
					*out = mlab->slots[x];
					mlab->slots[x] = 0;
					mlab->used--;
					return 1;
				}
			}
		}
	}
	
	*domain = 0;
	*out = 0;
	return 0;
}

/*
	@sdescription:	Will add a signal to the specified
					domain.
	@ldescription:	Will add a signal to the specified
					domain. Each domain can have at most
					the number of slots specified during
					initialization.
	@param:erg:		pointer to structure
	@param:domain:	domain value
	@param:signal:	signal as value
*/
int mla_add(MLA *sb, uintptr domain, uintptr signal) {
	MLAB		*mlab;
	uint32		x;
	
	/* look for block for domain */
	for (mlab = sb->blocks; mlab; mlab = mlab->next) {
		if (mlab->domain == domain) {
			/* add signal if room */
			for (x = 0; x < mlab->max; ++x) {
				if (!mlab->slots[x]) {
					mlab->slots[x] = signal;
					mlab->used++;
					return 1;
				}
			}
			/* no room */
			return 0;
		}
	}
	
	/* need to add domain block */
	mlab = (MLAB*)kmalloc(
		sizeof(MLAB) + sizeof(uintptr) * sb->dmax
	);
	memset(mlab, 0, sizeof(MLAB) + sizeof(uintptr) * sb->dmax);
	ll_add((void**)&sb->blocks, mlab);
	mlab->max = sb->dmax;
	mlab->domain = domain;
	mlab->slots[0] = signal;
	mlab->used = 1;
	return 1;
}
