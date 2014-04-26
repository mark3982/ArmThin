#!/usr/bin/python3
import sys
import struct

def attach(srcf, dstf, typf):
	#srcf = sys.argv[1]
	#dstf = sys.argv[2]
	#typf = sys.argv[3]

	sfd = open(srcf, "rb")
	sfd.seek(0, 2)
	sz = sfd.tell()
	sfd.seek(0, 0)
	sd = sfd.read(sz)
	sfd.close()

	pad = len(sd) & 3
	if pad > 0:
		pad = 4 - pad
	else:
		pad = 0

	dfd = open(dstf, "r+b")
	dfd.seek(0, 2)
	psz = dfd.tell()
	ppad = psz & 3
	if ppad > 0:
		ppad = 4 - ppad
	else:
		ppad = 0
	dfd.write(b'B' * ppad)
	dfd.write(struct.pack('IIII', sz + pad, 0x12345678, 0xedcba987, int(typf)))
	dfd.write(sd)
	#print('sz+pad=%s' % (sz + pad))
	if pad > 0:
		dfd.write(b'A' * pad)
	#print('[ATTACHMOD] spad:%s tpad:%s %s [%s bytes] ----> %s (old sz:%s new sz:%s)' % (pad, ppad, srcf, sz, dstf,  hex(psz), hex(dfd.tell())))
	dfd.close()
	return sz
