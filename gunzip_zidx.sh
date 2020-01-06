#!/bin/bash
FILES=$(ls -1 data/*/2018.01/bview.20180101.????.gz)

export TIMEFORMAT="%E"
echo "size,chunks,filename,time"
for j in 1 2 3
do
  for i in 0 2 3 4 5 6 7 8
  do
    for f in $FILES
    do
      echo "$(stat -c %s $f),$i,$f,$((time bin/gunzip_zidx $i $f $f.zx /dev/null) 2>&1)"
    done
  done
done
