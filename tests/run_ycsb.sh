#!/bin/bash
BUILDDIR=$(dirname "$0")/../build/
WORKLOADDIR=$(dirname "$0")/../include/ycsb/workloads/

function Run() {
    dbname=$1
    loadnum=$2
    opnum=$3
    scanop=$4
    thread=$5
    # ${WORKLOADDIR}/workloads_set.sh ${loadnum} ${opnum}

    ${BUILDDIR}/ycsb -db ${dbname} -threads ${thread} -P ${WORKLOADDIR} | tee ycsb-${dbname}-${thread}.txt
}

function run_all() {
    dbs="combotree fastfair pgm alex xindex"
    for dbname in $dbs; do
        echo "Run: " $dbname
        Run $dbname $1 $2 $3 1
        # sleep 100
    done
}
# DBName: combotree fastfair pgm xindex alex
# Run pgm 4000000 100000 10000 1
# for dbname
run_all 4000000 100000 10000 1