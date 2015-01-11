#ifndef CONFIG_H
#define CONFIG_H
/*
	============== CONFIGURATION =============

	These COULD and LIKELY WILL be board specific!

*/
/* the intial kernel stack and the exception stack */
#define KSTACKSTART 		0x2000		/* descending */
#define KSTACKEXC   		0x3000		/* descending */
/* somewhere to place the kernel state structure */
#define KSTARTSTATEADDR		0x3000
/* 
	RAM is assumed to start at 0x0, but we have to leave room for a little setup code, and
	depending on how much physical memory (KRAMSIZE) we are using we might have to adjust
	KRAMADDR to give a bit more. For example if KRAMSIZE is 4GB then KRAMADDR needs to be
	larger than 1MB, perferrably 2MB at least.
*/
#define KRAMADDR	0x10000
#define KRAMSIZE	(1024 * 1024 * 8)
/* 
	kernel virtual memory size 
	
	KMEMINDEX can be 1 through 7
	1 = 2GB
	2 = 1GB
	3 = 512MB
	4 = 256MB (MMIO problems; see note below)
	5 = 128MB (MMIO problems; see note below)
	6 = 64MB (MMIO problems; see note below)
	7 = 32MB (MMIO problems; see note below)
	
	This is the maximum amount of virtual memory per kernel space.
	
	Also to note. In my current code MMIO devices are identity mapped to lessen the
	changes when moving from a previous version of this source. If you go below 512MB
	you are going to have exceptions thrown so you will have to map them lower than 32MB
	and access them there. There is not a problem with mapping them lower I just did not
	do it. So I leave that as an excercise to the reader/user. The PIC, TIMER, and SERIAL
	are the three devices that need to be remapped and have their new addresses propagated
	by KSTATE or hardcoded.
*/
#define KMEMINDEX   3
#define KMEMSIZE	(0x1000000 << (8 - KMEMINDEX))
/* the size of a physical memory page */
#define KPHYPAGESIZE		1024
/* the size of a virtual page */
#define KVIRPAGESIZE		4096
/* the block size of the chunk heap (kmalloc, kfree) */
#define KCHKHEAPBSIZE		16
/* minimum block size for chunk heap (physical page count!) */
#define KCHKMINBLOCKSZ		5
/* block size for 1K page stack */
#define K1KPAGESTACKBSZ		1024
/* max number of ticks for a task */
#define KTASKTICKS 500000
/*
	============================================
*/
#endif