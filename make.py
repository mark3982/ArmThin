import sys
import os
import os.path
import subprocess

def __executecmd(cwd, args):
	p = subprocess.Popen(args, cwd = cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
	return (p.stdout.read().decode('utf-8'), p.stderr.read().decode('utf-8'))
	
	#so = []
	#se = []
	#so.append(p.stdout.read().decode('utf-8'))
	#se.append(p.stderr.read().decode('utf-8'))
	#while p.poll():
	#	so.append(p.stdout.read().decode('utf-8'))
	#	se.append(p.stderr.read().decode('utf-8'))	
	#return (''.join(so), ''.join(se))
	
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
	
def executecmd(cwd, args):
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
			if executecmd(dir, '%s %s -c %s %s' % (cfg['CC'], cfg['CCFLAGS'], node, hdrpaths)) is False:
				return (False, objs)
			objs.append('%s.o' % base)
	return (True, objs)

def makeModule(cfg, dir, out):
	print((bcolors.HEADER + bcolors.OKGREEN + 'module-make [%s]' + bcolors.ENDC) % (out))
		
	res, objs = compileDirectory(cfg, dir)
		
	objs = ' '.join(objs)
	if executecmd(dir, '%s -o ../%s.mod ../../corelib/core.o ../../corelib/rb.o %s' % (cfg['LD'], out, objs)) is False:
		return False
	pass
def makeBoard(cfg, dir, out):
	print((bcolors.HEADER + bcolors.OKGREEN + 'board-make [%s]' + bcolors.ENDC) % (out))
	
	res, objs = compileDirectory(cfg, dir)
	objs = ' '.join(objs)
	
	# use LD or AR
	if 'AR' in cfg:
		if executecmd(dir, '%s rvs %s %s' % (cfg['AR'], out, objs)) is False:
			return False
	else:
		if executecmd(dir, '%s -r -o %s %s' % (cfg['LD'], out, objs)) is False:
			return False
	return True
	
def compileCoreLIB(cfg, dir):
	pass
	
def configure():
	# test for needed binaries
	pass
	
def make(board, modules):
	cfg = {}
	cfg['CC'] = 'arm-eabi-gcc'
	cfg['LD'] = 'arm-eabi-ld'
	cfg['AR'] = 'arm-eabi-ar'
	cfg['OBJCOPY'] = 'arm-eabi-objcopy'
	cfg['CCFLAGS'] = '-mcpu=cortex-a9 -fno-builtin-printf -fno-builtin-sprintf -fno-builtin-memset'
	cfg['hdrpaths'] = ['../../']
	cfg['dirofboards'] = './boards'
	cfg['dirofmodules'] = './modules'
	cfg['board'] = board
	cfg['kimg'] = './armos.bin'
	cfg['ldflags'] = ''
	
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
		#makeBoard(		cfg = cfg,
		#				dir = '%s/%s' % (cfg['dirofboards'], bdir), 
		#				out = '../%s.board' % bdir,
		#)
		res, objs = compileDirectory(cfg = cfg, dir = '%s/%s' % (cfg['dirofboards'], bdir))
		if res is False:
			return False
		print(bdir, cfg['board'])
		if bdir == cfg['board']:
			bobjs = []
			for o in objs:
				bobjs.append('%s/%s/%s' % (cfg['dirofboards'], bdir, o))
			print(bobjs)
		
	# compile kernel
	makeKernel(cfg, dir = './', out = cfg['kimg'], bobjs = bobjs)

	# attach modules
	for mod in modules:
		if executecmd(dir, './attachmod.py %s %s 1' % ('%s/%s' % (dirofmodules, mod), cfg['kimg'])) is False:
			return False
	return True

def makeKernel(cfg, dir, out, bobjs):
	print(bcolors.HEADER + bcolors.OKGREEN + 'kernel-compile' + bcolors.ENDC)
	# compile all source files
	res, objs = compileDirectory(cfg, dir)
	if res is False:
		return False
	objs = ' '.join(objs)
	bobjs = ' '.join(bobjs)
	# link it
	if executecmd(dir, '%s -T link.ld -o __tmp.bin ./corelib/rb.o %s %s' % (cfg['LD'], objs, bobjs)) is False:
		return False
	# strip it
	if executecmd(dir, '%s -j .text -O binary __tmp.bin %s' % (cfg['OBJCOPY'], out)) is False:
		return False
	return True
	
make('realview-pb-a', [])