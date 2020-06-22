#!/bin/bash
gcc zidx.c -o -g zidx -lz -lstreamlike -lzidx
gdb --args zidx comp.gz comp_idx 25 1

