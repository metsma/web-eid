#!/usr/bin/env python3

import binascii
import sys

f = file(sys.argv[1], 'rb')
pem = bytearray(f.read())
# xor
for i in range(len(pem)):
    pem[i] = pem[i] ^ ((42 + i) % 255)
    
fo = file(sys.argv[1] + ".bin", 'wb')
fo.write(pem)
fo.close()

# unxor
for i in range(len(pem)):
    pem[i] = pem[i] ^ ((42 + i) % 255)

print pem
