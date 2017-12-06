#!/bin/bash
# After simulating with `enum.sh`
# it is expected some cases would fail
# due to server load.
# This script bridges such gap.

CARS="01 02 03 04 05"
WORKERS="001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017 018 019 020 030 050 064 072 086 100 120"
SPACES="001 002 003 004 005 006 007 008 009 010 011 012 013 014 015 016 017 018 019 020 030 040 050 100"
for car in $CARS
do
    for worker in $WORKERS
    do
	for space in $SPACES
	do
	    if [ ! $(cat logs/agg | grep "$car","$worker","$space") ]
	    then
		echo "record "$car"-"$worker"-"$space" not found, rerun..."
		./bench.out $car $worker $space
		sleep 5
	    fi
	done
    done
done

