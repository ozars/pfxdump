#!/bin/bash

COLLECTORS="rrc19 rrc11 rrc03 rrc10 rrc13 rrc15 rrc00 rrc12 rrc21 rrc20"
LOCAL_PATH=/media/omer/My\ Book1/ris/data.ris.ripe.net
PFXDUMP=./pfxdump
SNAPSHOT=2018.01/bview.20180101.0000.gz

function find_common_lines {
  awk 'NR==FNR { lines[$0]=1; next } $0 in lines' "$1" "$2"
}

function nonexisting_common_lines {
  local common=$1
  local searchin=$2
  return $(comm -23 <(sort -u $common) <(sort -u $searchin) | wc -l)
}

for c in $COLLECTORS; do
  $PFXDUMP /media/omer/My\ Book1/ris/data.ris.ripe.net/$c/$SNAPSHOT - 255.255.255.255/32 -i -d 2>&1 | grep "debug: " | sed -e 's/Prefix not found//' | sed -e 's/debug: //' > all_prefixes_$c.txt
done

i=0
for c in $COLLECTORS; do
  if [ $i -eq 0 ]; then
    cp "all_prefixes_$c.txt" "common_$((i+1)).txt"
  else
    find_common_lines "common_$i.txt" "all_prefixes_$c.txt" > common_$((i+1)).txt
    rm -f common_$i.txt
  fi
  ((i+=1))
done

mv -f common_$i.txt common_prefixes.txt

for c in $COLLECTORS; do
  nonexisting_common_lines common_prefixes.txt "all_prefixes_$c.txt"
  val=$?
  echo $c $val
  if [ $val -ne 0 ]; then
    echo "Error in $c"
    exit 1
  fi
  rm -f all_prefixes_$c.txt
done

lcount=$(($(wc -l common_prefixes.txt | cut -d " " -f 1) / 100))
echo $lcount
awk "NR%$lcount==0" common_prefixes.txt > sampled_prefixes.txt
