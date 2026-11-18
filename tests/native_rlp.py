#!/usr/bin/env python3
"""Independently decode actual signed SDK RLP for widths larger than host uint64."""
import subprocess, sys
chain=int(sys.argv[2]) if len(sys.argv)>2 else (1<<256)-1
raw=subprocess.check_output([sys.argv[1],'sign','22'*32,hex(chain),str(1<<40),'0x1','21000','0x'+'55'*20,'0x0','0x'],text=True)
b=bytes.fromhex(raw[2:])
def decode(i):
    x=b[i];i+=1
    if x<128:return bytes([x]),i
    if x<184:
        n=x-128;return b[i:i+n],i+n
    if x<192:
        k=x-183;n=int.from_bytes(b[i:i+k]);i+=k;return b[i:i+n],i+n
    if x<248:n=x-192
    else:k=x-247;n=int.from_bytes(b[i:i+k]);i+=k
    end=i+n;out=[]
    while i<end:v,i=decode(i);out.append(v)
    return out,i
fields,end=decode(0)
assert end==len(b) and len(fields)==9
assert int.from_bytes(fields[0])==1<<40
assert (int.from_bytes(fields[6])-35)//2==chain
assert fields[4]==b''
print('Independent RLP decode: full uint256 chain ID, 41-bit nonce, zero value PASS')
