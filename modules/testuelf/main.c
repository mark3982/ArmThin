#include "corelib/core.h"

int _start(unsigned int *smmio) {
	int				x;
	int				y;
	unsigned int	tps;
	
	tps = getTicksPerSecond();
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'G';
		smmio[0] = 'K';
		
		sleep(tps);
	}
	return 0;
}