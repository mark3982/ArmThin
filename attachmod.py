#!/usr/bin/python3

import sys
import struct

srcf = sys.argv[1]
dstf = sys.argv[2]
typf = sys.argv[3]

sfd = open(srcf, "rb")
sfd.seek(0, 2)
sz = sfd.tell()
sfd.seek(0, 0)
sd = sfd.read(sz)
sfd.close()

print('[ATTACHMOD] %s [%s bytes] ----> %s' % (srcf, sz, dstf))

dfd = open(dstf, "r+b")
dfd.seek(0, 2)
dfd.write(struct.pack('IIII', sz, 0x12345678, 0xedcba987, int(typf)))
dfd.write(sd)
dfd.close()
