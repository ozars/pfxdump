#!/bin/bash

YEAR=2018
MONTH=01
DAYS="01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30"
REMOTE_DAYS="01"
HOURS="0000 0800 1600"
COLLECTORS="rrc19 rrc11 rrc03 rrc10 rrc13 rrc15 rrc00 rrc12 rrc21 rrc20"
LOCAL_PATH=/media/omer/My\ Book1/ris/data.ris.ripe.net
REMOTE_PATH=http://data.ris.ripe.net

PFXDUMP=./pfxdump
SELECTED_PREFIXES=./sampled_prefixes.txt

export TIMEFORMAT=%R

function trigger_cache {
  local is_remote=$1
  local path=$2
  if [ $is_remote -eq 1 ]; then
    (wget -q "$path" -O- | head -c 1) >/dev/null 2>&1
  else
    (touch -a "$path") >/dev/null 2>&1
  fi
}

function print_header {
  echo 'remote,with_zx,prefix,day,hour,collector,time'
}

function print_benchmark {
  local is_remote=$1
  local mrt_basepath=$2
  local zx_basepath=$3
  local days=$4
  local times=$5
  for i in $(seq 1 $times); do
    while read prefix; do
      for d in $days; do
        for h in $HOURS; do
          for c in $COLLECTORS; do
            local mrt_relpath=/$c/$YEAR.$MONTH/bview.$YEAR$MONTH$d.$h.gz
            local mrt_path=$mrt_basepath/$mrt_relpath
            local zx=$zx_basepath/$mrt_relpath.zx
            trigger_cache 0 "$zx"
            trigger_cache $is_remote "$mrt_path"
            printf "$is_remote,1,$prefix,$MONTH/$d/$YEAR,$h,$c,"
            (time ${PFXDUMP} "$mrt_path" "$zx" "$prefix" >/dev/null 2>&1) 2>&1
            printf "$is_remote,0,$prefix,$MONTH/$d/$YEAR,$h,$c,"
            (time ${PFXDUMP} "$mrt_path" "$zx" "$prefix" -i >/dev/null 2>&1) 2>&1
          done
        done
      done
    done < $SELECTED_PREFIXES
  done
}

print_header
print_benchmark 0 "$LOCAL_PATH" "$LOCAL_PATH" "$DAYS" 1
print_benchmark 1 "$REMOTE_PATH" "$LOCAL_PATH" "$REMOTE_DAYS" 1
