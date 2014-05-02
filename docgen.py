import os
import sys
import pprint


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
	var			h;
	
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
codestates = 0

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
	cl_sp = [',', '*', '-', '+', '%', '^', '&', '~', '=', '!', '.', '>', '<', '(', ')', '<', '>', '{', '}', ';', ':', '?', '[', ']']
	
	tokens = []
	
	x = 0
	while True:
		# read up any non-white space
		blk, x = readthese(buf, x, cl_ws + cl_ge + cl_sp, [], False)
		blk = blk.strip()
		if len(blk) > 0:
			tokens.append(blk)
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
			if buf[x + 1] == '/':
				# read single line comment
				ei = buf.find('\n', x)
				blk = buf[x:ei]
				x = ei + 1
			elif buf[x + 1] == '*':
				# read block comment
				ei = buf.find('*/', x)
				blk = buf[x:ei + 2]
				x = ei + 1
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
	
def parseInfo(info):
	# drop all extra padding, spaces, and EOLs
	lsz = -1
	info = info.replace('/*', '')
	info = info.replace('*/', '')
	while len(info) != lsz:
		lsz = len(info)
		info = info.replace('\n', ' ')
		info = info.replace('  ', '')
		info = info.replace('	', '')
		info = info.replace('\r', '')
	parts = info.split('@')
	
	info = {}
	for part in parts:
		part = part.split(':')
		if len(part) < 2 and part[0] == '':
			continue
		_info = info
		__info = info
		__key = None
		x = 0
		while x < len(part) - 1:
			sp = part[x]
			_info[sp] = {}
			__info = _info
			__key = sp
			_info = _info[sp]
			x = x + 1
		if __key:
			__info[__key] = part[x]
		else:
			__info[part[0]] = True
	return info
	
def process(file):
	global forcnt, whilecnt, ifcnt, assigncnt, codestates

	d = readfile(file)
	
	out = {}
	out['calls'] = {}
	out['imps'] = {}
	out['calls-per-function'] = {}
	
	toks = tokenize(file)
	
	x = 1
	while x < len(toks):
		if toks[x] == '=' and toks[x - 1] != '=' and toks[x + 1] != '=':
			assigncnt = assigncnt + 1
		if toks[x] == ';':
			codestates = codestates + 1
		x = x + 1
			
	x = 0;
	while x < len(toks):
		if toks[x] == '{':
			# walk backwards
			y = x
			while y > -1:
				if toks[y] == '(':
					call, z = readthese(toks[y - 1], 0, [], [isvalidfuncname], True)
					print('got %s' % call)
					info = {}
					if len(call) > 0 and call not in ['for', 'white', 'if']:
						z = y
						while z > -1:
							if toks[z][0] == '}':
								break
							if toks[z][0:2] == '/*':
								info = toks[z]
								info = parseInfo(info)
								break
							z = z -1
					print('made info for %s' % call)
					out['imps'][call] = info
					break
				y = y - 1
			# walk through nested { } characters
			_call = call		# hehe bad fix
			nested = 1
			x = x + 1
			sub, x = grabnested(toks, x)
			# run through sub looking for calls
			y = 0
			print('@@@@%s' % _call)
			out['imps'][_call]['branch'] = 0
			out['imps'][_call]['ops'] = 0
			out['calls-per-function'][_call] = []
			while y < len(sub):
				# count branch type instructions
				if sub[y] in ['if', 'for', 'while', 'case']:
					out['imps'][_call]['branch'] = out['imps'][_call]['branch'] + 1
				# count major operations
				if sub[y] in ['+', '-', '*', '/', '%', '=', '~', '^', '&', '[', '>', '<']:
					out['imps'][_call]['ops'] = out['imps'][_call]['ops'] + 1
				# read this call
				if sub[y] == '(':
					call, z = readthese(sub[y - 1], 0, [], [isvalidfuncname], True)
					if len(call) > 0 and call not in ['for', 'while', 'if', 'switch', 'asm']:
						# track/count calls from this module (not per function)
						if call in out['calls']:
							out['calls'][call] = out['calls'][call] + 1
						else:
							out['calls'][call] = 1
						# track calls per function
						out['calls-per-function'][_call].append(call)
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

def buildtable(rows, tablestyle = ''):
	out = []
	
	out.append('<table style="%s">' % tablestyle)
	for row in rows:
		out.append('<tr>')
		for col in row:
			if type(col) in [list, tuple]:
				out.append(buildtable(col[0], tablestyle=col[1]))
				print('style', col[1])
			else:
				out.append('<td>%s</td>' % col)
		out.append('</tr>')
	out.append('</table>')
	return ''.join(out)
	

def gen(cfg, dir):
	meta = {}
	
	sblock = readsblock(sys.argv[0])
	
	gendir(meta, cfg, dir)
	
	fd = open('doc.htm', 'w')
	#fd = sys.stdout
	
	
	fd.write('<html><head><style>')
	fd.write('body { font-family: monospace; background: black; }')
	fd.write('.module { font-size:16pt; color: #ffaaaa; }')
	fd.write('.deps { font-size:12pt; color: #aaffaa; }')
	fd.write('.imps { font-size:10pt; color: #aaaaaa; }')
	fd.write('.sdescription { font-size:8pt; color: #aaaaaa; }')
	#fd.write('table { border: 1px line #cccccc; }')
	#fd.write('tr { border: 1px line #cccccc; }')
	#fd.write('td { border: 1px line #cccccc; }')
	fd.write('</style></head></body>')
	fd.write('<script language="javascript">%s</script>' % sblock);
	
	fc = 0
	cc = 0
	
	mtable = []
	
	fcg = {}
	ml = {}
	
	for m in meta:
		fmeta = meta[m]

		for imp in fmeta['imps']:
			fcg[imp] = {}
			for cff in fmeta['calls-per-function'][imp]:
				if cff in fcg[imp]:
					# backwards link
					fcg[imp][cff] = fcg[imp]
				else:
					# forward link
					fcg[imp][cff] = {}
					fcg[cff] = fcg[imp][cff]
	
	pprint.pprint(fcg)
	exit()
	
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
			
		# display imps
		mrow = []
		
		mrow.append('<span class="module">%s</span> ' % m);
		mrow.append('<span class="deps">%s</span>' % ' '.join(tmp))
		
		mtable.append(mrow)
		
		mrow = []
		tbl = []	
		tbl.append(['<b>FUNCTION</b>', '<b>BRANCH</b>', '<b>OPS</b>', '<b>DESCRIPTION</b>'])
		for call in fmeta['imps']:
			if len(call.strip()) < 1:
				continue
			row = []
			info = fmeta['imps'][call]
			if 'sdescription' in info:
				sdescription = info['sdescription']
			else:
				sdescription = ''
			fc = fc + 1
			row.append('<span class="imps">%s</span>' %  call)
			row.append('<span class="sdescription">%s</span>' % info['branch'])
			row.append('<span class="sdescription">%s</span>' % info['ops'])
			row.append('<span class="sdescription">%s</span>' % sdescription)
			tbl.append(row)
		
		if len(tbl) > 0:
			mrow.append([tbl, 'border: 1px line white;'])
		mtable.append(mrow)
		
	fd.write(buildtable(mtable, tablestyle='border: 1px line white;'))
		
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
print('codestates', codestates)