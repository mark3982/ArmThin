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
		print('\t\t[%s] %s' % (cwd, args))
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
			if not isNewerThan('%s/%s.o' % (dir, base), '%s/%s.c' % (dir, base)):
				print('    [CC] %s' % (node))
				if executecmd(dir, '%s %s -c %s %s' % (cfg['CC'], cfg['CCFLAGS'], node, hdrpaths), cmdshow=cfg['cmdshow']) is False:
					return (False, objs)
			else:
				print('    [GOOD] %s' % (node))
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
	# the -N switch has to prevent the file_offset in the elf32 from being
	# equal to the VMA address which was bloating the modules way too much
	# now they should be aligned to a 4K boundary or something similar
	if executecmd(dir, '%s -T ../../module.link -L%s -N -o ../%s.mod ../../corelib/linkhelper.o ../../corelib/linklist.o ../../corelib/kheap_bm.o ../../corelib/atomicsh.o ../../corelib/core.o ../../corelib/rb.o %s -lgcc' % (cfg['LD'], cfg['LIBGCCPATH'], out, objs), cmdshow=cfg['cmdshow']) is False:
		return False
	#if executecmd(dir, '%s -S ../%s.mod ../%s.mod' % (cfg['OBJCOPY'], out, out), cmdshow=cfg['cmdshow']) is False:
	#	return False
	pass
	
def compileCoreLIB(cfg, dir):
	print(bcolors.HEADER + bcolors.OKGREEN + 'CORELIB-compile' + bcolors.ENDC)
	res, objs = compileDirectory(cfg, dir)
	return res

def isNewerThan(obj, source):
	if os.path.exists(obj) is False:
		return False
	if os.path.getmtime(obj) > os.path.getmtime(source):
		return True
	return False

def makeKernel(cfg, dir, out, bobjs):
	print(bcolors.HEADER + bcolors.OKGREEN + 'kernel-compile' + bcolors.ENDC)
	# compile all source files
	old_ccflags = cfg['CCFLAGS'] 
	cfg['CCFLAGS'] = '%s -DKERNEL' % cfg['CCFLAGS']
	
	if executecmd(dir, '%s %s -c %s -o ./corelib/kheap_bm_kernel.o' % (cfg['CC'], cfg['CCFLAGS'], './corelib/kheap_bm.c'), cmdshow=cfg['cmdshow']) is False:
		cfg['CCFLAGS'] = old_ccflags
		return False
	
	res, objs = compileDirectory(cfg, dir)
	cfg['CCFLAGS'] = old_ccflags
	if res is False:
		return False
	
	
	# make sure main.o is linked first
	_objs = []
	for obj in objs:
		if obj != 'main.o':
			_objs.append(obj)
	objs = _objs
	
	objs = ' '.join(objs)
	bobjs = ' '.join(bobjs)
	
	tmp = '__armos.bin'
	# link it
	# %s/libgcc.a
	#  cfg['LIBGCCPATH'],-L%s
	if executecmd(dir, '%s -T link.ld -o %s main.o -L%s ./corelib/kheap_bm_kernel.o ./corelib/linklist.o ./corelib/atomicsh.o ./corelib/rb.o %s %s -lgcc' % (cfg['LD'], tmp, cfg['LIBGCCPATH'], objs, bobjs), cmdshow=cfg['cmdshow']) is False:
		return False
	# strip it
	if executecmd(dir, '%s -j .text -O binary %s %s' % (cfg['OBJCOPY'], tmp, out), cmdshow=cfg['cmdshow']) is False:
		return False
	return True
'''
	This is the main method to make the system.
'''
def make(cfg):
	board = cfg['board']
	modules= cfg['modules']
	# compile corelib
	if compileCoreLIB(cfg, dir = './corelib') is False:
		return False

	# special object used by modules
	# TODO: fix cant find CC executable
	#if executecmd('./', '%s %s -c %s %s' % (cfg['CC'], cfg['CCFLAGS'], 'atomicsh.c', ''), cmdshow=cfg['cmdshow']) is False:
	#	return False
	
	nodes = os.listdir(cfg['dirofmodules'])
	# compile modules
	for mdir in nodes:
		if os.path.isdir('%s/%s' % (cfg['dirofmodules'], mdir)) is False:
			continue
		res = makeModule(		cfg = cfg, 
								dir = '%s/%s' % (cfg['dirofmodules'], mdir), 
								out = mdir
		)
		if res is False:
			return False
		
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
		oldcc = cfg['CCFLAGS']
		cfg['CCFLAGS'] = '%s -DKERNEL' % cfg['CCFLAGS']
		res, objs = compileDirectory(cfg = cfg, dir = '%s/%s' % (cfg['dirofboards'], bdir))
		cfg['CCFLAGS'] = oldcc
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
	if makeKernel(cfg, dir = './', out = cfg['kimg'], bobjs = bobjs) is False:
		return False
	
	# attach modules
	for mod in modules:
		sz = attachmod.attach('%s/%s.mod' % (cfg['dirofmodules'], mod), cfg['kimg'], 1)
		print((bcolors.HEADER + bcolors.OKBLUE + 'attached [' + bcolors.OKGREEN + '%s' + bcolors.OKBLUE + '] @ %s bytes' + bcolors.ENDC) % (mod, sz))
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
cfg['CCFLAGS'] = '-save-temps -save-temps=cwd -O3 -mcpu=cortex-a9 -fno-builtin-free -fno-builtin-printf -fno-builtin-sprintf -fno-builtin-memset -fno-builtin-memcpy -fno-builtin-malloc'
cfg['hdrpaths'] = ['../../', '../', './corelib/']
cfg['dirofboards'] = './boards'
cfg['dirofmodules'] = './modules'
cfg['board'] = 'realview-pb-a'
cfg['modules'] = ['testuelf', 'fs']
cfg['kimg'] = './armos.bin'
cfg['ldflags'] = ''
cfg['cmdshow'] = False
cfg['LIBGCCPATH'] = '/home/kmcguire/opt/cross/lib/gcc/arm-eabi/4.8.2/'

def showHelp():
		print('%-20sdisplays list of modules' % 'showmodules')
		print('%-20sdisplays list of boards' % 'showboards')
		print('%-20sbuilds the system' % 'make <board> <modules..>')

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
		if len(sys.argv) < 3:
			print('missing <board> argument (first argument)')
			exit()
		cfg['board'] = sys.argv[2]
	
		mods = []
		x = 3
		while x < len(sys.argv):
			mods.append(sys.argv[x])
			x = x + 1
		cfg['modules'] = mods
		if make(cfg) is False:
			exit(-1)
		exit()
	showHelp()