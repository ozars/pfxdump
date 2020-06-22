#!/bin/bash
gcc zidx.c -o zidx -lz -lstreamlike -lzidx
./zidx comp.gz comp_idx 25 1
