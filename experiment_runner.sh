#!/bin/bash

YEAR=2018
MONTH=01
DAYS="01 02 03 04 05 06 07"
REMOTE_DAYS="01"
HOURS="0000 0800 1600"
COLLECTORS="rrc19 rrc11 rrc03 rrc10 rrc13 rrc15 rrc00 rrc12 rrc21 rrc20"
LOCAL_PATH=/media/omer/My\ Book1/ris/data.ris.ripe.net
REMOTE_PATH=http://data.ris.ripe.net
INTERFACE=enp0s31f6

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
  echo 'remote,with_zx,prefix,day,hour,collector,time,rx'
}

function run_pfxdump {
  local is_remote="$1"
  local mrt_path="$2"
  local zx="$3"
  local prefix="$4"
  if [ $is_remote -ne 0 ]; then
    (sudo nsntrace --dev $INTERFACE bash -c "export TIMEFORMAT=$TIMEFORMAT; time $PFXDUMP "\""$mrt_path"\"" "\""$zx"\"" "\""$prefix"\"" ${*:5} >/dev/null 2>&1" 2>&1 >/dev/null) 2>/dev/null | tr -d '\n'
    echo -n ","
    tshark -r nsntrace.pcap -T fields -e tcp.len 2>/dev/null | python3 -c "import sys; print(sum(int(l) for l in sys.stdin if len(l.strip())))"
  else
    (time $PFXDUMP "$mrt_path" "$zx" "$prefix" "${@:5}" >/dev/null 2>&1) 2>&1 | tr -d '\n'
    echo ",0"
  fi
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
            local mrt_relpath=$c/$YEAR.$MONTH/bview.$YEAR$MONTH$d.$h.gz
            local mrt_path=$mrt_basepath/$mrt_relpath
            local zx=$zx_basepath/$mrt_relpath.zx
            trigger_cache 0 "$zx"
            trigger_cache $is_remote "$mrt_path"
            printf "$is_remote,1,$prefix,$MONTH/$d/$YEAR,$h,$c,"
            run_pfxdump "$is_remote" "$mrt_path" "$zx" "$prefix"
            printf "$is_remote,0,$prefix,$MONTH/$d/$YEAR,$h,$c,"
            run_pfxdump "$is_remote" "$mrt_path" "$zx" "$prefix" -i
          done
        done
      done
    done < $SELECTED_PREFIXES
  done
}

print_header
print_benchmark 1 "$REMOTE_PATH" "$LOCAL_PATH" "$REMOTE_DAYS" 1
print_benchmark 0 "$LOCAL_PATH" "$LOCAL_PATH" "$DAYS" 3
