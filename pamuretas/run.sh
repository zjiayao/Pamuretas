#This is a sample bash script for q2.
#You can modify it for your benchmark experiments in q3

CARS="1 2 4"
SPACES="1 15 30"
WORKERS="1 2 8 30"
for cars in $CARS
do
    for spaces in $SPACES
    do
	for workers in $WORKERS
	do
	    ./tesla_factory.out $cars $spaces $workers
	done
    done
done

