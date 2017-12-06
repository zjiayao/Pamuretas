#!/bin/bash
# The tester driver (`bench.out`)
# save the results in `logs/`.
# This script is devoted to aggregate
# all results into a file named `agg`

if [[ -e agg ]]
then
    echo "agg existed!"
    exit
fi
touch agg
for log in `ls *.log`
do
    echo ${log:0:2},${log:3:3},${log:7:3},$(cat $log) >> agg
done


