int _start(unsigned int *smmio) {
	int		x;

	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'G';
		smmio[0] = 'K';
	}
	return 0;
}