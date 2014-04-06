int _start(unsigned int *smmio) {
	int		x;
	int		y;
	int		*a;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'G';
		smmio[0] = 'K';
		
		asm("swi #102");
	}
	return 0;
}