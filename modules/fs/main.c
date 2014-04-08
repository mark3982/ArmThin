int _start(unsigned int *smmio) {
	int		x;
	int		y;
	
	for(;;);
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'A';
	}
	return 0;
}