#!/bin/bash
# used to drive the tester
# given number of cars.
# the tester will enumerate the
# worker-space combinations
CARS="1 2 3 4 5 6 7 8 9 10 15 30 50"
for car in $CARS
do
    ./bench.out $car
    sleep 15m
done

