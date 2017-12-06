OBJS = main.o job.o worker.o
CC = gcc
DEBUG = -g
CFLAGS = -Wall -fopenmp -pthread -O2
DFLAG =
debug = 'off'
ifeq ($(debug), on)
    DFLAG = -DDEBUG
endif

all: pamuretas

bench: loader.c
	$(CC) $(CFLAGS) $(DFLAG) loader.c job.o worker.o -o benchmark

solver: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(DFLAG) -o pamuretas

main.o: definitions.h main.h main.c
	$(CC) $(CFLAGS) $(DFLAG) -c main.c

worker.o: worker.h job.h definitions.h worker.c
	$(CC) $(CFLAGS) $(DFLAG) -c worker.c

job.o: definitions.h job.h job.c
	$(CC) $(CFLAGS) $(DFLAG) -c job.c

clean:
	rm *.o *.out
