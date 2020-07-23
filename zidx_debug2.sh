#!/bin/bash
gcc zidx_user_modify.c -O0 -g -o zidx_user_modify -lz -lstreamlike -lzidx
rm ./testfiles/odyssey.gz
cp ./testfiles/odyssey_original.gz ./testfiles/odyssey.gz
gdb --args zidx_user_modify ./testfiles/odyssey.gz ./testfiles/odyssey_idx 25 1

