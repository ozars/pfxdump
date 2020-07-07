from zlib import crc32
import time
import string
import random
import sys
def crc32_combine(a,b,lenb):
	c=b'\x00'*lenb
	return crc32(c,a) ^ b ^ crc32(c)
def crc32_extract(a,ab,lenb):
	c=b'\x00' * lenb
	return ab ^ crc32(c) ^ crc32(c,a)

def ripple_checksum(idx, c_list, d_list):
	if(idx<len(c_list)-1):
		next_checksum = crc32_extract(c_list[idx],c_list[idx+1],len(d_list[idx+1]))
		new_checksum = crc32(d_list[idx])
		c_list[idx] = crc32_combine(c_list[idx-1],new_checksum, len(d_list[idx]))
		c_list[idx+1] = crc32_combine(c_list[idx],next_checksum,len(d_list[idx+1]))
		ripple_checksum(idx+1,c_list,d_list)
	else:
		print("Ripple results ",c_list)

def naive_checksum_update(idx, c_list, d_list):
	while(idx<len(d_list)):
		if(idx==0):
			c_list[idx]=crc32(d_list[idx])
		else:
			c_list[idx]=crc32_combine(c_list[idx-1],crc32(d_list[idx]),len(d_list[idx]))
		idx=idx+1
	print("Naive results: ",c_list)
def trivial_checksum_update(idx,c_list,d_list):
	s=b''
	i=0
	for x in d_list:
		s=s+x
		c_list[i]=crc32(s)
		i=i+1
	print("Trivial results: ",c_list)
def build_list(stringlen=32768,listlen=10):
	ret=[]
	for i in range(listlen):
		letters=string.ascii_lowercase
		ret.append(bytes(''.join(random.choice(letters) for i in range(stringlen)),'ascii'))
	return ret
sys.setrecursionlimit(10**6)
v=build_list()
#print("Before modification: ",v)
crclist=[]
s=b''
for x in v:
	s=s+x
	crclist.append(crc32(s))

idx=2
letters=string.ascii_lowercase
v[idx]=bytes(''.join(random.choice(letters) for i in range(32768)),'ascii')
#print("After modification: ",v)
rip_start=time.time()
ripple_checksum(idx,crclist,v)
rip_end=time.time()

naive_start=time.time()
naive_checksum_update(idx,crclist,v)
naive_end=time.time()

trivial_start=time.time()
trivial_checksum_update(idx,crclist,v)
trivial_end=time.time()
print("Ripple time:   ",rip_end-rip_start)
print("Naive time:    ",naive_end-naive_start)
print("Trivial time:  ",trivial_end-trivial_start)


