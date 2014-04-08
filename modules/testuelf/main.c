unsigned int getTicksPerSecond() {
	unsigned int			out;
	asm("	swi #103 \n\
			mov %[out], r0 \n\
		" : [out]"=r" (out));
	return out;
}

void sleep(unsigned int timeout) {
	asm("	mov r0, %[in] \n\
			swi #101 \n\
		" : : [in]"r" (timeout));
}

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