#!/bin/bash
BUILDDIR=$(dirname "$0")/../build/
Loadname="longlat-400m"
function Run() {
    Loadname="longlat-400m"
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5
    # gdb --args \
    numactl --cpubind=1 --membind=1 ${BUILDDIR}/scalability_test --dbname ${dbname} --load-size ${loadnum} \
        --put-size ${opnum} --get-size ${opnum} --delete-size ${opnum}\
        -t $thread --loadstype 1 | tee scalability-nvm-write-${dbname}-${Loadname}.txt
}

function Run_LGN() {
    Loadname="lognormal-150m"
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5
    # gdb --args \
    numactl --cpubind=1 --membind=1 ${BUILDDIR}/scalability_test --dbname ${dbname} --load-size ${loadnum} \
        --put-size ${opnum} --get-size ${opnum} --delete-size ${opnum}\
        -t $thread --loadstype 2 | tee scalability-nvm-write-${dbname}-${Loadname}.txt
}

function Run_YCSB() {
    Loadname="ycsb-400m"
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5
    # gdb --args \
    numactl --cpubind=1 --membind=1 ${BUILDDIR}/scalability_test --dbname ${dbname} --load-size ${loadnum} \
        --put-size ${opnum} --get-size ${opnum} --delete-size ${opnum}\
        -t $thread --loadstype 3 | tee scalability-nvm-write-${dbname}-${Loadname}.txt
}

# DBName: combotree fastfair pgm xindex alex
function run_all() {
    dbs="lipp xindex"
    for dbname in $dbs; do
        echo "Run: LTD" $dbname
        Run $dbname $1 $2 $3 1
        sleep 100
    done
}
function run_lgn_all() {
    dbs="lipp letree fastfair pgm xindex"
    for dbname in $dbs; do
        echo "Run: LGN" $dbname
        Run_LGN $dbname $1 $2 $3 1
        sleep 100
    done
}
function run_ycsb_all() {
    dbs="letree fastfair alex pgm xindex"
    for dbname in $dbs; do
        echo "Run: YCSB" $dbname
        Run_YCSB $dbname $1 $2 $3 1
        sleep 100
    done
}

function main() {
    dbname="combotree"
    loadnum=4000000
    opnum=100000
    scansize=4000000
    thread=1
    if [ $# -ge 1 ]; then
        dbname=$1
    fi
    if [ $# -ge 2 ]; then
        loadnum=$2
    fi
    if [ $# -ge 3 ]; then
        opnum=$3
    fi
    if [ $# -ge 4 ]; then
        scansize=$4
    fi
    if [ $# -ge 5 ]; then
        thread=$5
    fi
    if [ $dbname == "all" ]; then
        run_all $loadnum $opnum $scansize $thread
    fi
    if [ $dbname == "all_lgn" ]; then
        run_lgn_all $loadnum $opnum $scansize $thread
    fi
    if [ $dbname == "all_ycsb" ]; then
        run_ycsb_all $loadnum $opnum $scansize $thread
    else
        echo "Run $dbname $loadnum $opnum $scansize $thread"
        Run $dbname $loadnum $opnum $scansize $thread
    fi 
}
main letree 400000000 1000000 4000000 1
# main all_lgn 150000000 1000000 4000000 1
# main all_ycsb 400000000 1000000 4000000 1