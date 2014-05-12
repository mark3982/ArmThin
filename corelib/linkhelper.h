#ifndef CORELIB_H_LINKHELPER
#define CORELIB_H_LINKHELPER
#include <main.h>
#include <corelib/core.h>
#include <corelib/rb.h>

typedef int (*LH_WRITE_NBIO)(void *hdr, void *p, uint32 sz);
typedef int (*LH_READ_NBIO)(void *hdr, void *p, uint32 *sz);
typedef int (*LH_PEEK_NBIO)(void *hdr, void *p, uint32 *sz, uint8 **mndx);

/*
	@sdescription:		Represents a uni/bi-directional IPC link.
*/
typedef struct _CORELIB_LINK {
	/* linked list members */
	struct _CORELIB_LINK		*next;
	struct _CORELIB_LINK		*prev;
	/* members */
	//ERH				tx;
	//ERH				rx;
	void			*tx;
	void			*rx;
	
	uintptr			txsize;
	uintptr			rxsize;
	
	uint32			txesz;			/* static message size TX */
	uint32			rxesz;			/* static message size RX */
	
	LH_WRITE_NBIO	wnbio;			/* call for specific protocol */
	LH_READ_NBIO	rnbio;			/* call for specific protocol */
	LH_PEEK_NBIO	pnbio;			/* call for specific protocol */
	
	uintptr			addr;			/* address of first page of link */
	uintptr			pcnt;			/* number of pages comprising the link */
	uintptr			process;		/* remote process identifier */
	uintptr			thread;			/* remote thread identifier */
	uintptr			rsignal;		/* remote signal value */
	uintptr			lsignal;		/* local signal value */
	
	void			*extra;			/* extra data */
} CORELIB_LINK;

typedef int (*LH_PKTARRIVED)(void *arg, CORELIB_LINK *link);
typedef int (*LH_LINKREQ)(void *arg, uintptr process, uintptr thread, uint32 proto);
typedef int (*LH_LINKDROPPED)(void *arg, CORELIB_LINK *link);
typedef int (*LH_LINKESTABLISHED)(void *arg, CORELIB_LINK *link);
typedef int (*LH_KMSG)(void *arg, uint32 *pkt, uint32 sz);
typedef int (*LH_LINKFAILED)(void *arg, uint32 rid);

typedef struct _CORELIB_LINKHELPER {
	/* @sdescription:		Root link to linked list of link structures. */
	CORELIB_LINK		*root;	
	/* @sdescription:		Maximum slots in array. Can be changed by allocating new array. */
	uint32				arraymax;
	/* @sdescription:		Handler for arrival of packets. */
	LH_PKTARRIVED		handler_pktarrived;
	/* @sdescription:		Handler for link request. */
	LH_LINKREQ			handler_linkreq;
	LH_LINKDROPPED		handler_linkdropped;
	LH_LINKESTABLISHED	handler_linkestablished;
	LH_KMSG				handler_kmsg;
	LH_LINKFAILED		handler_linkfailed;
	void				*handler_arg;
	
	char				*dbgname;
	
	/*
		@sdescription: 		Fast maps signal to corelib link.
		@ldescription:		The signal directly indexs into this array which
							holds a pointer to the actual link structure. This
							array is resized and copied as needed for expansion,
							but links always occupy the same signal slot.
	*/
	CORELIB_LINK		**array;
} CORELIB_LINKHELPER;

/* generic write/read/peek */
int lh_write_nbio(CORELIB_LINK *link, void *p, uint32 sz);
int lh_read_nbio(CORELIB_LINK *link, void *p, uint32 *sz);
int lh_peek_nbio(CORELIB_LINK *link, void *p, uint32 *sz, uint8 **mndx);

/* callback setters */
void lh_setlinkfailed(LH_LINKFAILED h);
void lh_setkmsg(LH_KMSG h);
void lh_setpktarrived(LH_PKTARRIVED h);
void lh_setlinkreq(LH_LINKREQ h);
void lh_setlinkdropped(LH_LINKDROPPED h);
void lh_setlinkestablished(LH_LINKESTABLISHED h);
void lh_setoptarg(void *arg);

/* extra */
void lh_setextra(CORELIB_LINK *link, void *extra);
void *lh_getextra(CORELIB_LINK *link);

/* utility */
int lh_establishlink(uint32 rprocess, uint32 rthread, uint32 proto, uint32 rxsz, uint32 txsz, uint32 rxesz, uint32 txesz, uint32 rid);
int er_waworr(ERH *tx, ERH *rx, void *out, uint32 sz, uint32 rid32ndx, uint32 rid, uint32 timeout);
int er_worr(ERH *rx, void *out, uint32 sz, uint32 rid32ndx, uint32 rid, uint32 timeout);

/* debug */
void lh_setdbgname(char *dbgname);

/* internal */
uint32 lh_getnewsignalid();

/* core functions */
int lh_init();
int lh_sleep(uint32 timeout);
int lh_tick();
#endif