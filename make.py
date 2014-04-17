import sys
import os
import os.path
import subprocess
import attachmod

'''
	COMMAND LINE SUPPORT
'''

def __executecmd(cwd, args):
	p = subprocess.Popen(args, cwd = cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
	return (p.stdout.read().decode('utf-8'), p.stderr.read().decode('utf-8'))
	
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
	
def executecmd(cwd, args, cmdshow=True):
	if cmdshow:
		print('\t\t%s' % args)
	args = args.split(' ')

	so, se = __executecmd(cwd, args)
	
	no = []
	parts = se.split('\n')
	for part in parts:
		no.append('\t%s\n' % part)
	no = ''.join(no)
	
	if len(no.strip()) > 0:
		print(bcolors.HEADER + bcolors.WARNING + no + bcolors.ENDC)
	
	if len(se) > 0:
		return False
	return True

'''
	GENERIC DIRECTORY COMPILATION WITH GCC COMPATIBLE COMPILER
'''

def compileDirectory(cfg, dir):
	nodes = os.listdir(dir)
	
	_hdrpaths = []
	for hdrpath in cfg['hdrpaths']:
		_hdrpaths.append('-I%s' % hdrpath)
	hdrpaths = ' '.join(_hdrpaths)
	
	objs = []
	for node in nodes:
		if node.find('.') < 1:
			continue
		ext = node[node.find('.') + 1:]
		base = node[0:node.find('.')]
		if ext == 'c':
			print('    [CC] %s' % (node))
			if executecmd(dir, '%s %s -c %s %s' % (cfg['CC'], cfg['CCFLAGS'], node, hdrpaths), cmdshow=cfg['cmdshow']) is False:
				return (False, objs)
			objs.append('%s.o' % base)
	return (True, objs)

'''
	SPECIFIC BUILD SUPPORT
	
	Certain parts of the system are built a differenty, and these
	provide a solution to these in a modular way.
'''
def makeModule(cfg, dir, out):
	print((bcolors.HEADER + bcolors.OKGREEN + 'module-make [%s]' + bcolors.ENDC) % (out))
		
	res, objs = compileDirectory(cfg, dir)
		
	objs = ' '.join(objs)
	if executecmd(dir, '%s -o ../%s.mod ../../corelib/core.o ../../corelib/rb.o %s' % (cfg['LD'], out, objs), cmdshow=cfg['cmdshow']) is False:
		return False
	pass
	
def compileCoreLIB(cfg, dir):
	pass

def makeKernel(cfg, dir, out, bobjs):
	print(bcolors.HEADER + bcolors.OKGREEN + 'kernel-compile' + bcolors.ENDC)
	# compile all source files
	res, objs = compileDirectory(cfg, dir)
	if res is False:
		return False
	objs = ' '.join(objs)
	bobjs = ' '.join(bobjs)
	# link it
	if executecmd(dir, '%s -T link.ld -o __tmp.bin ./corelib/rb.o %s %s' % (cfg['LD'], objs, bobjs), cmdshow=cfg['cmdshow']) is False:
		return False
	# strip it
	if executecmd(dir, '%s -j .text -O binary __tmp.bin %s' % (cfg['OBJCOPY'], out), cmdshow=cfg['cmdshow']) is False:
		return False
	return True
'''
	This is the main method to make the system.
'''
def make(cfg):
	board = cfg['board']
	modules= cfg['modules']
	# compile corelib
	compileCoreLIB(cfg, dir = './corelib')
		
	nodes = os.listdir(cfg['dirofmodules'])
	# compile modules
	for mdir in nodes:
		if os.path.isdir('%s/%s' % (cfg['dirofmodules'], mdir)) is False:
			continue
		makeModule(		cfg = cfg, 
						dir = '%s/%s' % (cfg['dirofmodules'], mdir), 
						out = mdir
		)
		
	nodes = os.listdir(cfg['dirofboards'])
	# compile boards
	for bdir in nodes:
		if os.path.isdir('%s/%s' %(cfg['dirofboards'], bdir)) is False:
			continue
		# at the moment i just compile all boards but in the future this could
		# be changed to compile just the target board because some boards could
		# be architecture specific use inline assembly which would fail and clutter
		# up the output with errors.. unless we change the way we handle things
		print((bcolors.HEADER + bcolors.OKGREEN + 'board-make [%s]' + bcolors.ENDC) % (bdir))
		res, objs = compileDirectory(cfg = cfg, dir = '%s/%s' % (cfg['dirofboards'], bdir))
		if res is False:
			return False
		# if this board module is to be included in the kernel then
		# we want to track the object files produced so we can directly
		# include them into the kernel linking process
		if bdir == cfg['board']:
			bobjs = []
			for o in objs:
				bobjs.append('%s/%s/%s' % (cfg['dirofboards'], bdir, o))
		
	# compile kernel
	makeKernel(cfg, dir = './', out = cfg['kimg'], bobjs = bobjs)
	
	# attach modules
	for mod in modules:
		sz = attachmod.attach('%s/%s.mod' % (cfg['dirofmodules'], mod), cfg['kimg'], 1)
		print((bcolors.HEADER + bcolors.OKBLUE + 'attached [' + bcolors.OKGREEN + '%s' + bcolors.OKBLUE + '] @ %s bytes' + bcolors.ENDC) % (bdir, sz))
	return True

def readfile(path):
	fd = open(path, 'r')
	d = fd.read().strip()
	fd.close()
	return d
	
cfg = {}
# configuration specific (may want to take these from environment vars if existing)
cfg['CC'] = 'arm-eabi-gcc'
cfg['LD'] = 'arm-eabi-ld'
cfg['AR'] = 'arm-eabi-ar'
cfg['OBJCOPY'] = 'arm-eabi-objcopy'
cfg['CCFLAGS'] = '-mcpu=cortex-a9 -fno-builtin-printf -fno-builtin-sprintf -fno-builtin-memset'
cfg['hdrpaths'] = ['../../']
cfg['dirofboards'] = './boards'
cfg['dirofmodules'] = './modules'
cfg['board'] = 'realview-pb-a'
cfg['modules'] = ['testuelf', 'fs']
cfg['kimg'] = './armos.bin'
cfg['ldflags'] = ''
cfg['cmdshow'] = False

def showHelp():
		print('help!!')

if len(sys.argv) < 2:
	showHelp()
else:
	farg = sys.argv[1]

	if farg == 'showmodules':
		nodes = os.listdir(cfg['dirofmodules'])
		for node in nodes:
			if os.path.isdir('%s/%s' % (cfg['dirofmodules'], node)) is False:
				continue
			info = readfile('%s/%s/info' % (cfg['dirofmodules'], node))
			print((bcolors.HEADER + bcolors.OKGREEN + '%-20s%s' + bcolors.ENDC) % (node, info))
		exit()
	if farg == 'showboards':
		nodes = os.listdir(cfg['dirofboards'])
		for node in nodes:
			if os.path.isdir('%s/%s' % (cfg['dirofboards'], node)) is False:
				continue
			info = readfile('%s/%s/info' % (cfg['dirofboards'], node))
			print((bcolors.HEADER + bcolors.OKGREEN + '%-20s%s' + bcolors.ENDC) % (node, info))
		exit()
	if farg == 'make':
		make(cfg)
		exit()