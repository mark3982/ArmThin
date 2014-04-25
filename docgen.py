import os
import sys


'''
$START$
/*
	==== STYLE RANDOMIZER ===
	Just copy and paste this entire section, and also leave my name intact:
	Leonard Kevin McGuire Jr
*/
function componentToHex(c) {
	var hex = c.toString(16);
	return hex.length == 1 ? "0" + hex : hex;
}

function rgbToHex(r, g, b) {
	return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b);
}

function randomhtmlcolor(max) {
	var r, g, b;
	r = Math.floor(Math.random() * max);
	g = Math.floor(Math.random() * max);
	b = Math.floor(Math.random() * max);
	return rgbToHex(r, g, b);
}

function randomizestyle() {
	var			i, x, ss;
	
	for (i = 0; i < document.styleSheets.length; ++i) {
		if (document.styleSheets[i]['rules'])
			ss = document.styleSheets[i]['rules'];
		else if (document.styleSheets[i]['cssRules'])
			ss = document.styleSheets[i]['cssRules'];
		for (x = 0; x < ss.length; ++x) {
			if (!ss[x].style['ignore']) {
				ss[x].style['color'] = randomhtmlcolor(256);
				ss[x].style['background'] = randomhtmlcolor(30);
			}
		}
	}
}
randomizestyle();
$END$
'''

assigncnt = 0
lcnt = 0
ccnt = 0
ifcnt = 0
forcnt = 0
whilecnt = 0
comcnt = 0

def readsblock(path):
	fd = open(path, 'r')
	lines = fd.readlines()
	fd.close()
	
	out = None
	
	for line in lines:
		if line.find('$END$') == 0:
			break
		if out is not None:
			out.append(line)
		if line.find('$START$') == 0:
			out = []
	return '\n'.join(out)

def readfile(path):
	global lcnt, ccnt
	
	fd = open(path, 'rb')
	d = fd.read().decode('utf8', 'ignore').strip()
	fd.close()

	lines = d.split('\n')
	
	for line in lines:
		line = line.strip()
		if len(line) > 0:
			lcnt = lcnt + 1
			for c in line:
				if c != ' ' and c != '\t' and c != '\n':
					ccnt = ccnt + 1
	return d
	
def isvalidfuncname(c):
	if c >= 'a' and c <= 'z':
		return True
	if c >= 'A' and c <= 'Z':
		return True
	if c >= '0' and c <= '9':
		return True
	if c == '_':
		return True
	return False
	
def isofthese(what, these, thesef):
	for this in these:
		if what == this:
			return True
	for f in thesef:
		if f(what):
			return True
	return False

def readthese(buf, start, these, thesef, expect=True):
	out = []
	x = start
	while x < len(buf):
		if isofthese(buf[x], these, thesef) == expect:
			out.append(buf[x])
		else:
			return (''.join(out), x)
		x = x + 1
	return (''.join(out), x)
	
def tokenize(file):
	global comcnt

	buf = readfile(file)
	
	cl_ws = [' ', '\n', '\t']
	cl_eol = ['\n']
	cl_ge = ['"', "'", '/']
	cl_sp = [',', '*', '-', '+', '%', '^', '&', '~', '=', '!', '.', '>', '<', '(', ')', '<', '>', '{', '}', ';', ':', '?']
	
	tokens = []
	
	x = 0
	while True:
		# read up any non-white space
		blk, x = readthese(buf, x, cl_ws + cl_ge + cl_sp, [], False)
		if len(blk) > 0:
			tokens.append(blk)
		#s	print(blk, end=' ')
		blk = None
		
		if x >= len(buf):
			break
		
		if buf[x] in cl_sp:
			blk = buf[x]
			x = x + 1
		elif buf[x] == '"':
			# read until terminating quote
			blk, x = readthese(buf, x + 1, ['"'], [], False)
			blk = '"%s"' % blk			# add beginning quote back
			x = x + 1
		elif buf[x] == "'":
			blk, x = readthese(buf, x + 1, ["'"], [], False)
			blk = "'%s'" % blk			# add beginning quote back
			x = x + 1
		elif buf[x] == '/':
			if x + 1 >= len(buf):
				return tokens
			if buf[x + 1] == '*':
				# read comment
				ei = buf.find('*/', x)
				x = ei + 1
				blk = buf[x:ei + 2]
				comcnt = comcnt + 1
			else:
				blk = '/'
				x = x + 1
		if blk is not None:
			tokens.append(blk)
		#	print(blk, end='')
		# read up any whitespace
		tmp, x = readthese(buf, x, cl_ws, [], True)
	return tokens
	
def grabnested(toks, x):
	nested = 1
	out = []
	while nested > 0 and x < len(toks):
		# while working out way out pick up any calls
		if toks[x] == '{':
			nested = nested + 1
		if toks[x] == '}':
			nested = nested - 1
		out.append(toks[x])
		x = x + 1
	return (out, x)
	
def process(file):
	global forcnt, whilecnt, ifcnt, assigncnt

	d = readfile(file)
	
	out = {}
	out['calls'] = {}
	out['imps'] = {}
	
	toks = tokenize(file)
	
	x = 1
	while x < len(toks):
		if toks[x] == '=' and toks[x - 1] != '=' and toks[x + 1] != '=':
			assigncnt = assigncnt + 1
		x = x + 1
			
	x = 0;
	while x < len(toks):
		if toks[x] == '{':
			# walk backwards
			y = x
			while y > -1:
				if toks[y] == '(':
					call, z = readthese(toks[y - 1], 0, [], [isvalidfuncname], True)
					if len(call) > 0 and call not in ['for', 'white', 'if']:
						out['imps'][call] = (None, None)
					break
				y = y - 1
			# walk through nested { } characters
			nested = 1
			x = x + 1
			sub, x = grabnested(toks, x)
			# run through sub looking for calls
			y = 0
			while y < len(sub):
				if sub[y] == '(':
					call, z = readthese(sub[y - 1], 0, [], [isvalidfuncname], True)
					if len(call) > 0 and call not in ['for', 'while', 'if']:
						if call in out['calls']:
							out['calls'][call] = out['calls'][call] + 1
						else:
							out['calls'][call] = 1
					else:
						if call == 'for':
							forcnt = forcnt + 1
						if call == 'while':
							whilecnt = whilecnt + 1
						if call == 'if':
							ifcnt = ifcnt + 1
				y = y + 1
			# okay outside body again
			
		x = x + 1
	return out
	
def gendir(meta, cfg, dir, level = 0):
	levelpad = '   '
	nodes = os.listdir(dir)
	for node in nodes:
		# ignore hidden
		if node[0] == '.':
			continue
		# handle directories
		if os.path.isdir('%s/%s' % (dir, node)):
			print('%sgoing into %s' % (level * levelpad, node))
			gendir(meta, cfg, '%s/%s' % (dir, node), level = level + 1)
		# handle different file types
		ext = node[node.find('.') + 1:]
		base = node[0:node.find('.')]
		# process C source files
		if ext == 'c':
			print('%sprocessing %s for metadata' % (level * levelpad, node))
			
			nsname = ('%s/%s' % (dir, base)).replace('/', '.')
			
			while nsname.find('..') > -1:
				nsname = nsname.replace('..', '.')
			
			if nsname[0] == '.':
				nsname = nsname[1:]
			
			meta[nsname] = process('%s/%s' % (dir, node))
	
def gen(cfg, dir):
	meta = {}
	
	sblock = readsblock(sys.argv[0])
	
	gendir(meta, cfg, dir)
	
	fd = open('doc.htm', 'w')
	#fd = sys.stdout
	
	
	fd.write('<html><head><style>')
	fd.write('body { background: black; }')
	fd.write('.module { font-size:16pt; color: #ffaaaa; }')
	fd.write('.deps { font-size:12pt; color: #aaffaa; }')
	fd.write('.imps { font-size:8pt; color: #aaaaaa; }')
	fd.write('</style></head></body>')
	fd.write('<script language="javascript">%s</script>' % sblock);
	
	fc = 0
	cc = 0
	
	for m in meta:
		fmeta = meta[m]
		# determine dependancies
		fmeta['deps'] = {}
		print('looking at %s' % (m))
		for call in fmeta['calls']:
			cc = cc + fmeta['calls'][call]
			print('    finding call %s' % call)
			# find it
			for _m in meta:
				if _m == m:
					continue
				if call in meta[_m]['imps']:
					if _m in fmeta['deps']:
						fmeta['deps'][_m] = fmeta['deps'][_m] + 1
					else:
						fmeta['deps'][_m] = 1
		tmp = []
		for dep in fmeta['deps']:
			tmp.append('%s:%s' % (dep, fmeta['deps'][dep]))
		fd.write('<span class="module">%s</span> <span class="deps">%s</span></br>\n' % (m, ' '.join(tmp)))
		# display imps
		for call in fmeta['imps']:
			fc = fc + 1
			fd.write('\t\t<span class="imps">%s</span> \n' % call)
		fd.write('</br>')
	fd.write('</body></html>')
	fd.close()
	
	print('fc', fc)
	print('cc', cc)
	return
	
	
cfg = {}
gen(cfg, './')

print('lcnt', lcnt)
print('ccnt', ccnt)
print('forcnt', forcnt)
print('whilecnt', whilecnt)
print('ifcnt', ifcnt)
print('comcnt', comcnt)
print('assigncnt', assigncnt)