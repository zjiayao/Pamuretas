#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
// #define DEBUG

// global constants and variables
#define PROGRAM "./tesla_factory.out"
#define NUM_COMPONENTS 8
#define WORKER_FREE    0
#define WORKER_WORKING 1
#define WORKER_PRODGRP 2
#define WORKER_ASSMGRP 4
#define JOB_UNFINISHED 0
#define JOB_FINISHED   1
#define TASK_NORMAL    0
#define TASK_CRITICAL  1

// scheduler management
#define PRODUCTION_FAILURES 10
#define PRODUCTION_FAILURES_HARD 2000
#define ASSMEMBLY_FAILURES  10
#define SANITY_CHECK_NEEDED 200
#define NONSANITY 20
#define PRODUCTION_WAIT 300  // production line waiting time
#define ASSEMBLY_WAIT   3000 // assembly line waiting time
#define SCHEDULER_WAIT	5000 // scheduler waiting time

/*---------Do Not Change Anything Below This Line---------*/
//Job ID
#define SKELETON  0
#define ENGINE    1
#define CHASSIS   2
#define BODY      3
#define WINDOW    4
#define TIRE      5
#define BATTERY   6
#define CAR       7

/*-----Production time for each item-----*/
// Phase 1
#define TIME_SKELETON 5
#define TIME_ENGINE   4
#define TIME_CHASSIS  3
#define TIME_BODY     4

// Phase 2
#define TIME_WINDOW   1
#define TIME_TIRE     2
#define TIME_BATTERY  3
#define TIME_CAR      6

/*---------Do Not Change Anything Above This Line---------*/

// bit representation for masking
// in the `check_availability()`
#define B_SKELETON  1 << SKELETON
#define B_ENGINE    1 << ENGINE
#define B_CHASSIS   1 << CHASSIS
#define B_BODY	    1 << BODY
#define B_WINDOW    1 << WINDOW
#define B_TIRE	    1 << TIRE
#define B_BATTERY   1 << BATTERY
#define B_CAR	    1 << CAR
