import	sys


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

'''
	Yes, this is very inefficent reading one byte
	at a time, but it works with decent speed to
	get the job done. I just need to color the
	output to make it a tad bit easier to read.
'''

colortbl = {}
cndx = 90

buf = []
while True:
	raw = sys.stdin.read(1)
	buf = '%s%s' % (buf, raw)
	while buf.find('\n') > -1:
		line = buf[0:buf.find('\n')]
		buf = buf[buf.find('\n') + 1:]
		
		if len(line) < 1:
			print('')
			continue
		
		if line[0] == '\t':
			print(line)
		
		if line[0] == '[':
			mod = line[1:line.find(']')]
			if mod not in colortbl:
				colortbl[mod] = '\033[%sm' % cndx
				cndx = cndx + 1
			line = line[line.find(']')+1:]
			print(bcolors.HEADER + colortbl[mod], end='')
			print('[%s]' % mod, end='')
			print(bcolors.ENDC, end='')
			print(line)
	