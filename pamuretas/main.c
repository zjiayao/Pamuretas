#include "definitions.h"
#include "main.h"
#include <pthread.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

// semaphores
sem_t sem_worker;
sem_t sem_space;

sem_t sem_skeleton;
sem_t sem_engine;
sem_t sem_chassis;
sem_t sem_body;

sem_t sem_window;
sem_t sem_tire;
sem_t sem_battery;
sem_t sem_car;

// helper semaphores
sem_t sem_prod_workers, sem_assm_workers;
sem_t sem_worker_status_register;

// global variables
int num_cars = 0;
int num_spaces = 0;
int num_workers = 0;
int num_prod_workers = 0, num_assm_workers = 0;
int num_spaces_accrual = 0;
resource_pack *rpack = NULL;
int *workers_status;

// aux threads
pthread_t *workers, production_dispatcher, assembly_dispatcher, scheduler;

// scheduler variable and semaphores
int expected_time = 0;
int task_status = TASK_NORMAL;
int critical_space = 3;
int prod_trigger = 0, assm_trigger = 0, prod_cnt = 0, assm_cnt = 0;
sem_t sem_prod_trigger, sem_assm_trigger, sem_prod_cnt, sem_assm_cnt, sem_space_accrual;

// book-keeping
int num_deadlocks = 0;
double total_production_time = 0.0;

// queues
work_queue production_queue;
work_queue assembly_queue;

// helpers
void init_work_queues(int);
void destroy_work_queues();
int check_availability(int);
int request_free_worker(int);
void return_free_worker(int, int);
void *production_dispatch(void *arg);
void *assembly_dispatch(void *arg);
void *schedule(void *arg);
int allocate_worker_quota();
void enable_production();
void enable_assembly();

// increment the space accrued
inline void incr_accrual_space(int amt) {
    sem_wait(&sem_space_accrual);
    num_spaces_accrual += amt;
    sem_post(&sem_space_accrual);
}

// reset production line failure count
inline void reset_prod_cnt() {
    sem_wait(&sem_prod_cnt);
    prod_cnt = 0;
    sem_post(&sem_prod_cnt);
}

// reset assembly line failure count
inline void reset_assm_cnt() {
    sem_wait(&sem_assm_cnt);
    assm_cnt = 0;
    sem_post(&sem_assm_cnt);
}

// test and reset production trigger and failure count
inline int cr_production() {
    int res;
    sem_wait(&sem_prod_trigger);
    if ( (res = prod_trigger) ) {
	prod_trigger = 0;
    }
    sem_post(&sem_prod_trigger);
    if (res) {
	sem_wait(&sem_prod_cnt);
	prod_cnt = 0;
	sem_post(&sem_prod_cnt);
#ifdef DEBUG
	fprintf(stderr, "[Scheduler] Production line override\n");
#endif
    }
    return res;
}

// test and reset assembly trigger and failure count
inline int cr_assembly() {
    int res;
    sem_wait(&sem_assm_trigger);
    if ( (res = assm_trigger) ) {
	assm_trigger = 0;
    }
    sem_post(&sem_assm_trigger);
    if(res) {
	sem_wait(&sem_assm_cnt);
	assm_cnt = 0;
	sem_post(&sem_assm_cnt);
#ifdef DEBUG
	fprintf(stderr, "[Scheduler] Assembly line override\n");
#endif
    }
    return res;
}

// increment production line failure count
// noted ++ may be non-atomic on some platform
// hence we guard it
inline void incr_prod_cnt() {
    sem_wait(&sem_prod_cnt);
    ++prod_cnt;
    sem_post(&sem_prod_cnt);
}

// increment assembly line failure count
inline void incr_assm_cnt() {
    sem_wait(&sem_assm_cnt);
    ++assm_cnt;
    sem_post(&sem_assm_cnt);
}

// check if a queue is empty
inline int is_empty(work_queue *queue) {
    return queue->begin >= queue->end;
}

// deque
inline work_pack *dequeue(work_queue *queue) {
    assert(queue->end > queue->begin);
    return &queue->queue[queue->begin++];
}

// enqueue and set work_pack
inline void enqueue(work_queue *queue, int jid, int times) {
    work_pack *wpack = &queue->queue[queue->end++];
    wpack->jid = jid;
    wpack->times = times;
    wpack->resource = rpack;
    wpack->tid = 0;
    wpack->gid = -1;
    wpack->status = JOB_UNFINISHED;
#ifdef DEBUG
    fprintf(stderr, "Enqueued, current size: %d\n", queue->end);
#endif
}

// check if all jobs in a queue are finished
inline int is_terminate(work_queue *queue) {
    for(int i = 0; i != queue->end; ++i) {
	if( queue->queue[i].status == JOB_UNFINISHED ) {
	    return 0;
	}
    }
    return 1;
}

// check if any more spaces
// considering the accrual spaces
// return 0 if there is there is some space
int check_space() {
    int spaces;
    sem_wait(&sem_space_accrual);
    spaces = num_spaces_accrual;
    sem_post(&sem_space_accrual);
#ifdef DEBUG
    fprintf(stderr, "[CHKSPC] Total Space: %d    Accrued spaces: %d\n", num_spaces,  spaces);
#endif
    return (num_spaces - spaces) < critical_space;
}

// check if any free workers currently available
inline int check_free_worker(int group) {
    int cnt = 0;
    switch(group) {
	case WORKER_PRODGRP:
	    sem_getvalue(&sem_prod_workers, &cnt);
	    break;
	case WORKER_ASSMGRP:
	    sem_getvalue(&sem_assm_workers, &cnt);
	    break;
	default:
#ifdef DEBUG
	    fprintf(stderr, "[Warning] Invalid group id encountered in check_free_worker, ignored\n");
#endif
	    break;
    }
    return cnt > 0;
}

// condition for production line to pause
inline int prod_wait() {
    return (num_workers > 10) && check_space();
}

// change main into another function
// for benchmarking purposes
#ifdef BENCH
double bench(int nc, int nw, int ns, char buf[128])
#else
int main(int argc, char** argv)
#endif
{
#ifdef BENCH
    num_cars = nc;
    num_workers = nw;
    num_spaces = ns;
#else
    if (argc < 4) {
	printf("Usage: %s <number of cars> <number of spaces> <number of workers>\n",
	argv[0]);
	return EXIT_SUCCESS;
	num_cars     = 1;
	num_spaces   = 20;
	num_workers  = 8;
    } else {
	num_cars     = atoi(argv[1]);
	num_spaces   = atoi(argv[2]);
	num_workers  = atoi(argv[3]);
	if(argc == 6) {
	    num_deadlocks = atoi(argv[4]);
	    sscanf(argv[5], "%lf", &total_production_time);
	}
    }
#endif

    // test if the task configuration is tractable and
    // set quota for the production and assembly line
    if (allocate_worker_quota() == -1) {
#ifdef BENCH
	FILE *fout = fopen(buf, "w");
	fprintf(fout, "-1\n");
	fclose(fout);
	return -1;
#else
	exit(1);
#endif
    }

    printf("[Pamuretas] Job defined, %d workers will build %d cars with %d storage spaces\n",
		    num_workers, num_cars, num_spaces);

    // prepare and put semaphores into resource_pack
    rpack = (struct resource_pack*) malloc(sizeof(struct resource_pack));
    initResourcePack(rpack, num_spaces, num_workers);

    // prepare queues
    init_work_queues(num_cars);

    // prepare workers
    workers = (pthread_t*) malloc(sizeof(pthread_t) * num_workers);
    workers_status = (int*) malloc(sizeof(int) * num_workers);
    memset(workers_status, WORKER_FREE, sizeof(int) * num_workers);

#ifdef DEBUG
    fprintf(stderr, "Initialization Completed:\nproduction tasks: %d    assembly tasks: %d\n", production_queue.end - production_queue.begin, assembly_queue.end - assembly_queue.begin);
#endif

    // start working and time the whole process
    double production_time = omp_get_wtime();
    register int rc;
    if ((rc = pthread_create(&scheduler, NULL, schedule, &production_time))) {
	fprintf(stderr, "[Error] Failure creating scheduler\n");
	return EXIT_FAILURE;
    }

    // wait for scheduler
    // the dispatchers and workers are
    // being waited therein
    pthread_join(scheduler, NULL);

    // log result
    production_time = omp_get_wtime() - production_time + total_production_time;
    reportResults(production_time);

    // clean all up
    destroySem();
    destroy_work_queues();
    free(rpack);
    free(workers);
#ifdef BENCH
    FILE* fout = fopen(buf, "w");
    fprintf(fout, "%f\n", production_time);
    fclose(fout);
    return production_time;
#else
    return EXIT_SUCCESS;
#endif
}

// sheduler thread
// mediate between production and assembly
// line dispatchers
void *schedule(void *arg) {
    // dispatch dispatchers
    double production_time = *((double*) arg);
    register int rc;
    int snapshot[NUM_COMPONENTS] = {0};
    int sanity_check_cnt = 0;
    int nonsanity_cnt = 0;
    int secnod_chance = 1;
#ifdef DEBUG
    fprintf(stderr, "Scheduler activated\n");
#endif

    // create dispatchers
    if ((rc = pthread_create(&production_dispatcher, NULL, production_dispatch, NULL))) {
	fprintf(stderr, "[Error] Failure creating production_dispatcher\n");
	EXIT_FAILURE;
    }
    if ((rc = pthread_create(&assembly_dispatcher, NULL, assembly_dispatch, NULL))) {
	fprintf(stderr, "[Error] Failure creating assembly_dispatcher\n");
	EXIT_FAILURE;
    }

    // momentum
    if(task_status == TASK_NORMAL) {
	enable_production();
	usleep(500);
	enable_assembly();
	usleep(500);
    }

    // monitor the dispatchers and workers
    int* cars_produced = (int*) malloc(sizeof(int));
    sem_getvalue(&sem_car, cars_produced);
    while(*cars_produced < num_cars) {
	// the values at this instance
	int prod_fail_cnts = prod_cnt,
	    assm_fail_cnts = assm_cnt;
#ifdef DEBUG
	fprintf(stderr, "[Monitor] Cars produced: %d    Production line failures: %d    Assembly line failures: %d\n", *cars_produced, prod_fail_cnts, assm_fail_cnts);

	int *sem_value = (int*) malloc(sizeof(int));
	sem_getvalue(&sem_skeleton, sem_value);
	fprintf(stderr, "\tSkeleton: %d\n",   *sem_value);
	sem_getvalue(&sem_engine,   sem_value);
	fprintf(stderr, "\tEngine: %d\n",     *sem_value);
	sem_getvalue(&sem_chassis,  sem_value);
	fprintf(stderr, "\tChassis: %d\n",    *sem_value);
	sem_getvalue(&sem_body,     sem_value);
	fprintf(stderr, "\tBody: %d\n",       *sem_value);
	sem_getvalue(&sem_window,   sem_value);
	fprintf(stderr, "\tWindow: %d\n",     *sem_value);
	sem_getvalue(&sem_tire,     sem_value);
	fprintf(stderr, "\tTire: %d\n",       *sem_value);
	sem_getvalue(&sem_battery,  sem_value);
	fprintf(stderr, "\tBattery: %d\nFree Workers: {",    *sem_value);
	free(sem_value);
	for(int i = 0; i < num_workers; ++i)
	{
	    if(workers_status[i] == WORKER_FREE) {
		fprintf(stderr, "%d ", i);
	    }
    	}
	fprintf(stderr, "}\n");

#endif

	// too many failures for production
	// due to limited space enable assembly line even
	// pre-maturely if possible
	if(num_workers > 1 && prod_fail_cnts > PRODUCTION_FAILURES)
	{
	    enable_assembly();
	    // or due to space limitation
	    if(prod_fail_cnts > PRODUCTION_FAILURES_HARD) {
		enable_production();
	    }
	    usleep(100);
	}

	// failure of the assembly line may due to inavailable
	// materials, which in turn may result from the limited
	// spaces (assume normal mode).
	if(num_workers > 1 && assm_fail_cnts > ASSMEMBLY_FAILURES)
	{
	    int cur_assm_workers = 0;
	    sem_getvalue(&sem_assm_workers, &cur_assm_workers);
	    if(check_availability(B_ENGINE | B_SKELETON | B_CHASSIS) || check_availability(B_BODY | B_TIRE | B_WINDOW | B_BATTERY)) {
		if(cur_assm_workers > 0) {
		    enable_assembly();
		}
	    } else {
		if(cur_assm_workers > 0) {
		    enable_assembly();
		} else {
		    if (!check_space()) {
			enable_production();
		    }
		}
	    }
	    usleep(100);
	}

	// whether to perform sanity check
	if(expected_time && ++sanity_check_cnt > SANITY_CHECK_NEEDED) {
#ifdef DEBUG
	    fprintf(stderr, "[Scheduler] Performing sanity check... ");
#endif
	    int dirty = 0;
	    sanity_check_cnt = 0;

	    // compare current state with snapshot
	    int current_snapshot[NUM_COMPONENTS] = {0};
	    sem_getvalue(&sem_skeleton, current_snapshot);
	    sem_getvalue(&sem_engine,   current_snapshot+1);
	    sem_getvalue(&sem_chassis,  current_snapshot+2);
	    sem_getvalue(&sem_body,     current_snapshot+3);
	    sem_getvalue(&sem_window,   current_snapshot+4);
	    sem_getvalue(&sem_tire,     current_snapshot+5);
	    sem_getvalue(&sem_battery,  current_snapshot+6);
	    if(nonsanity_cnt == 0) {
		for(int k = 0; k < NUM_COMPONENTS; ++k) {
		    snapshot[k] = current_snapshot[k];
		}
	    } else {
		for(int k = 0; k < NUM_COMPONENTS; ++k) {
		    if(snapshot[k] != current_snapshot[k]) {
			dirty = 1;
		    } else {
			snapshot[k] = current_snapshot[k];
		    }
		}
	    }
	    ++nonsanity_cnt;
	    nonsanity_cnt *= 1 - dirty;
#ifdef DEBUG
	    fprintf(stderr, "%s(%d)\n", dirty ? "healthy" : "non-healthy", nonsanity_cnt);
#endif

	    // check if possible deadlock
	    if(NONSANITY && expected_time && nonsanity_cnt >= NONSANITY * num_cars) {
		double time_elapse = omp_get_wtime() - production_time;
#ifdef DEBUG
		fprintf(stderr, "[Scheduler] Time Elapsed: %f, proceed to non-sanity handler\n", time_elapse);
#endif
		// second chance
		if(secnod_chance-- > 0) {
		    fprintf(stdout, "[Warning] Deadlock may occur, on time: %f sec(s)\n", time_elapse);
		    nonsanity_cnt = 0;
		} else {
		    fprintf(stdout, "[Fatal] Threads not responding for %f sec, revamping...\n", time_elapse);
		    char arg_cars[256], arg_spaces[256], arg_workers[256], arg_total_time[32], arg_num_deadlock[32];

		    sprintf(arg_cars, "%d", num_cars);
		    sprintf(arg_spaces, "%d", num_spaces);
		    if(num_spaces >= 13) {
			num_workers = num_workers > 1 ? num_workers - 1 : 1;
		    }
		    sprintf(arg_workers, "%d", num_workers);
		    sprintf(arg_total_time, "%12f", total_production_time + time_elapse);
		    sprintf(arg_num_deadlock, "%d", ++num_deadlocks);
		    char *const newargs[] = {PROGRAM, arg_cars, arg_spaces, arg_workers, arg_num_deadlock, arg_total_time, '\0'};

		    if(!fork()) {
			execv(PROGRAM, newargs);
		    }
		    exit(-1);
		}
	    }

	}
	usleep(SCHEDULER_WAIT);
	sem_getvalue(&sem_car, cars_produced);
    }

    // wait for dispatchers (though unnecessary)
    pthread_join(production_dispatcher, NULL);
    pthread_join(assembly_dispatcher, NULL);

    free(cars_produced);
#ifdef DEBUG
    fprintf(stderr, "[Scheduler] Terminated\n");
#endif
    pthread_exit(NULL);
}

// production_dispatcher
void *production_dispatch(void *arg) {
    // task: dispatch and finish the production queue
    while(!is_empty(&production_queue)) {
	while(prod_wait()) {
#ifdef DEBUG
	    fprintf(stderr, "[Production] waiting...\n");
#endif
	    usleep(PRODUCTION_WAIT);

	    // if is signaled to proceed
	    if(check_free_worker(WORKER_PRODGRP)) {
#ifdef DEBUG
	    fprintf(stderr, "[Production] Free worker available, this incident is reported\n");
#endif
		if(cr_production()) {
		    break;
		}
		incr_prod_cnt();
	    }
	}
	reset_prod_cnt();

	// fetch and dispatch the top task
	work_pack *wpack = dequeue(&production_queue);
	int tid = request_free_worker(WORKER_PRODGRP);
	wpack->tid = tid;
	wpack->gid = WORKER_PRODGRP;
#ifdef DEBUG
	fprintf(stderr, "[Production] Job %d(x%d) is to dispatch to worker %d\n", wpack->jid, wpack->times, wpack->tid);
#endif
	register int rc;
	if ((rc = pthread_create(&workers[tid], NULL, work, wpack))) {
	    fprintf(stderr, "[Error] Failure creating worker %d under production dispatcher\n", tid);
	    exit(1);
	}

	// log accrualed space
	incr_accrual_space(wpack->times);

	// special treatment if resources are critical
	if(task_status == TASK_CRITICAL) {
	    switch(wpack->jid) {
		case CHASSIS:
		    // single worker case
		    // proper timing is essential
		    if(num_workers == 1) {
			sem_wait(&sem_prod_workers);
			sem_post(&sem_assm_workers);
		    } else {
			enable_assembly();
    			sem_wait(&sem_body);
			sem_post(&sem_body);
		    }
		    break;
		case BATTERY:
		    if(num_workers == 1) {
			sem_wait(&sem_prod_workers);
			sem_post(&sem_assm_workers);
		    }
		    enable_assembly();
		    break;
		default:
		    break;
	    }
	    enable_assembly();
	    usleep(100);
	}
    }

    // check whether all job done
    while(!is_terminate(&production_queue)) {
	usleep(500);
    }
    usleep(10);

    // release free worker quota to assembly line
    int *quota = (int*) malloc(sizeof(int));
    sem_getvalue(&sem_prod_workers, quota);
    for(int i = 0; i < *quota; ++i) {
	sem_post(&sem_assm_workers);
    }
    free(quota);

#ifdef DEBUG
    fprintf(stderr, "[Production] Terminated\n");
#endif

    pthread_exit(NULL);
}

// assembly_dispatcher
void *assembly_dispatch(void *arg) {
    // task is to dispatch and empty the assembly queue
    while(!is_empty(&assembly_queue)) {
	// assemble BODY
	while(!check_availability(B_ENGINE | B_SKELETON | B_CHASSIS)) {
#ifdef DEBUG
	    fprintf(stderr, "[Assembly] waiting...\n");
#endif
	    usleep(ASSEMBLY_WAIT);
	    if(check_free_worker(WORKER_ASSMGRP)) {
#ifdef DEBUG
	    fprintf(stderr, "[Assembly] Free worker available, this incident is reported\n");
#endif
		if(cr_assembly()) {
		    break;
		}
		incr_assm_cnt();
	    }
	}
	reset_assm_cnt();

	// dispatch the top task
	work_pack *wpack_a = dequeue(&assembly_queue);
	int tid_a = request_free_worker(WORKER_ASSMGRP);
	wpack_a->tid = tid_a;
	wpack_a->gid = WORKER_ASSMGRP;
#ifdef DEBUG
	fprintf(stderr, "[Assembly] Job %d(x%d) is to dispatch to worker %d\n", wpack_a->jid, wpack_a->times, wpack_a->tid);
#endif
	register int rc;
	if ((rc = pthread_create(&workers[tid_a], NULL, work, wpack_a))) {
	    fprintf(stderr, "[Error] Failure creating worker %d under assembly dispatcher\n", tid_a);
	    exit(1);
	}

	// since one body yields one space but clears
	// three spaces
	incr_accrual_space(-2 * wpack_a->times);
	if(num_workers == 1) {
	    sem_wait(&sem_assm_workers);
	    sem_post(&sem_prod_workers);
	    enable_production();
	}

	usleep(10);

	// assemble CAR
	while(!check_availability(B_BODY | B_TIRE | B_WINDOW | B_BATTERY)) {
#ifdef DEBUG
	    fprintf(stderr, "[Assembly] waiting...\n");
#endif
	    usleep(ASSEMBLY_WAIT);
	    if(check_free_worker(WORKER_ASSMGRP)) {
#ifdef DEBUG
	    fprintf(stderr, "[Assembly] Free worker available, this incident is reported\n");
#endif
		if(cr_assembly()) {
		    break;
		}
		incr_assm_cnt();
	    }
	}
	reset_assm_cnt();
	work_pack *wpack_b = dequeue(&assembly_queue);
	int tid_b = request_free_worker(WORKER_ASSMGRP);
	wpack_b->tid = tid_b;
	wpack_b->gid = WORKER_ASSMGRP;
#ifdef DEBUG
	fprintf(stderr, "[Assembly] Job %d(x%d) is to dispatch to worker %d\n", wpack_b->jid, wpack_b->times, wpack_b->tid);
#endif
	if ((rc = pthread_create(&workers[tid_b], NULL, work, wpack_b))) {
	    fprintf(stderr, "[Error] Failure creating worker %d under assembly dispatcher\n", tid_b);
	    exit(1);
	}
	incr_accrual_space(-12 * wpack_b->times);
	if(num_workers == 1) {
	    sem_wait(&sem_assm_workers);
	    sem_post(&sem_prod_workers);
	    enable_production();
	    usleep(10);
	}
    }

    // check whether all job done
    while(!is_terminate(&assembly_queue)) {
	usleep(500);
    }
    usleep(10);

    // release free worker quota to production line
    int *quota = (int*) malloc(sizeof(int));
    sem_getvalue(&sem_assm_workers, quota);
    for(int i = 0; i < *quota; ++i) {
	sem_post(&sem_prod_workers);
    }
    free(quota);

#ifdef DEBUG
    fprintf(stderr, "[Assembly] Terminated\n");
#endif
    pthread_exit(NULL);
}

// validate job configuration and
// set few parameters accordingly
int allocate_worker_quota() {
    if (num_workers == 1) {
	// single threaded minimal space: 13
	if (num_spaces < 13) {
	    printf("[Fatal] Single worker requires at least 13 space but only %d given, abort\n", num_spaces);
	    return -1;
	}
	num_prod_workers = 1;
	num_assm_workers = 0;
	task_status = TASK_CRITICAL;
	prod_trigger = 1;
	assm_trigger = 0;
    } else {
	// we roughly distribute the workers among two lines
	// by the ratio of 3 : 1
	num_assm_workers = (int)(.25 * num_workers) < 1 ? 1 : (int)(.25 * num_workers);
	num_prod_workers = num_workers - num_assm_workers;

	if (num_spaces < 13 || num_workers < 13) {
	    num_assm_workers = num_assm_workers > 2 ? 2 : num_assm_workers;
	    num_prod_workers = num_workers - num_assm_workers;
	    task_status = TASK_CRITICAL; assm_trigger = prod_trigger = 1;
	}
	expected_time = num_cars * 60;

    }
#ifdef DEBUG
    fprintf(stderr, "Prodcution: %d worker(s)    Assembly: %d worker(s)\n", num_prod_workers, num_assm_workers);
#endif
    return 0;
}

// get a free worker
int request_free_worker(int gid) {
    switch(gid) {
	case WORKER_PRODGRP:
	    sem_wait(&sem_prod_workers);
	    break;
	case WORKER_ASSMGRP:
	    sem_wait(&sem_assm_workers);
	    break;
	default:
	    fprintf(stderr, "[Fatal] Logic error in worker group id in request_free_worker\n");
	    exit(1);
    }
    int idx = -1;
    while(idx == -1) {
	// this while loop is unnecessary
	// as each line keeps a saparate wokre
	// count
	sem_wait(&sem_worker_status_register);
	for(int i = 0; i < num_workers; ++i) {
	    if (workers_status[i] == WORKER_FREE) {
		idx = i;
		workers_status[idx] = WORKER_WORKING;
		sem_post(&sem_worker_status_register);
#ifdef DEBUG
	        fprintf(stderr, "[Request] A free worker %d found\n", idx);
#endif
		return idx;
	    }
	}
	sem_post(&sem_worker_status_register);
	usleep(50);
    }
    fprintf(stderr, "[Fatal] Logic error at request_free_worker\n");
    exit(1);
}

// change a worker to available
void return_free_worker(int idx, int gid) {
    sem_wait(&sem_worker_status_register);
    workers_status[idx] = WORKER_FREE;
    switch(gid) {
	case WORKER_PRODGRP:
	    sem_post(&sem_prod_workers);
	    break;
	case WORKER_ASSMGRP:
	    sem_post(&sem_assm_workers);
	    break;
	default:
	    fprintf(stderr, "[Fatal] Logic error in worker group id\n");
	    exit(1);
    }
    sem_post(&sem_worker_status_register);
}

// check the availability of materials
// specified by the bit mask
// (defined in the `definition.h`)
// by default, tires and windows are available
// iff they reached the prescribed amount
int check_availability(int mask) {
    int flag = 1;
    int *value = (int*) malloc(sizeof(int));
    for(int i = 0; i < NUM_COMPONENTS; ++i) {
	if(mask & (1 << i)) {
	    switch(i) {
		case SKELETON:
		    sem_getvalue(&sem_skeleton, value);
		    flag &= (*value > 0);
		    break;
		case ENGINE:
		    sem_getvalue(&sem_engine, value);
		    flag &= (*value > 0);
		    break;
		case CHASSIS:
		    sem_getvalue(&sem_chassis, value);
		    flag &= (*value > 0);
		    break;
		case BODY:
		    sem_getvalue(&sem_body, value);
		    flag &= (*value > 0);
		    break;
		case WINDOW:
		    sem_getvalue(&sem_window, value);
		    flag &= (*value >= 7);
		    break;
		case TIRE:
		    sem_getvalue(&sem_tire, value);
		    flag &= (*value >= 4);
		    break;
		case BATTERY:
		    sem_getvalue(&sem_battery, value);
		    flag &= (*value > 0);
		    break;
		case CAR:
		    sem_getvalue(&sem_car, value);
		    flag &= (*value > 0);
		    break;
		default:
		    fprintf(stderr, "[Fatal] Logic error in check_availability\n");
		    free(value);
		    exit(1);
	    }

	    if (!flag) {
		free(value);
		return 0;
	    }
	}
    }
    free(value);
    return 1;
}

// initialize the work queues
void init_work_queues(int goal) {
#ifdef DEBUG
	printf("Initiating queues, goal: %d...\n", goal);
#endif
    // each goal consists of a skeleton
    // a chassis, an engine, a battery and
    // seven windows and four tires
    production_queue.queue = (work_pack*) malloc(sizeof(work_pack) * goal * 15);

    // each goal consists of a body and a car
    assembly_queue.queue = (work_pack*)malloc(sizeof(work_pack) * goal * 2);

    for(int i = 0; i < goal; ++i) {
	enqueue(&production_queue, SKELETON, 1);
	enqueue(&production_queue, ENGINE, 1);
	enqueue(&production_queue, CHASSIS, 1);
	for(int j = 0; j < 7; ++j) {
	    enqueue(&production_queue, WINDOW, 1);
	}
	for(int j = 0; j < 4; ++j) {
	    enqueue(&production_queue, TIRE, 1);
	}
	enqueue(&production_queue, BATTERY, 1);

	enqueue(&assembly_queue, BODY, 1);
	enqueue(&assembly_queue, CAR, 1);
    }
#ifdef DEBUG
	printf("Init queues done!\n");
	// assignee should all be 0
	for(int i = production_queue.begin; i != production_queue.end; ++i) {
	    fprintf(stderr, "Job %d (x%d) assigned to %d\n", production_queue.queue[i].jid, production_queue.queue[i].times, production_queue.queue[i].tid);
	}
	for(int i = assembly_queue.begin; i != assembly_queue.end; ++i) {
	    fprintf(stderr, "Job %d (x%d) assigned to %d\n", assembly_queue.queue[i].jid, assembly_queue.queue[i].times, assembly_queue.queue[i].tid);
	}
#endif

}

// free memories in the work queues
inline void destroy_work_queues() {
    free(production_queue.queue);
    free(assembly_queue.queue);
}

// report result
void reportResults(double production_time) {
	int *sem_value = (int*) malloc(sizeof(int));
	printf("==============Final report===========\n");
	printf("=======Jiayao Zhang (3035233412)=====\n");
	printf("===========jyzhang@cs.hku.hk=========\n");

	sem_getvalue(&sem_skeleton, sem_value);
	printf("Unused Skeleton: %d\n",   *sem_value);
	sem_getvalue(&sem_engine,   sem_value);
	printf("Unused Engine: %d\n",     *sem_value);
	sem_getvalue(&sem_chassis,  sem_value);
	printf("Unused Chassis: %d\n",    *sem_value);
	sem_getvalue(&sem_body,     sem_value);
	printf("Unused Body: %d\n",       *sem_value);
	sem_getvalue(&sem_window,   sem_value);
	printf("Unused Window: %d\n",     *sem_value);
	sem_getvalue(&sem_tire,     sem_value);
	printf("Unused Tire: %d\n",       *sem_value);
	sem_getvalue(&sem_battery,  sem_value);
	printf("Unused Battery: %d\n",    *sem_value);

	sem_getvalue(&sem_space, sem_value);
	if (*sem_value < num_spaces) {
		printf("There are waste car parts!\n");
	}
	sem_getvalue(&sem_car, sem_value);
	printf("Production: %d %s with %d %s\nProduction time: ",
		*sem_value,
		*sem_value > 1 ? "cars" : "car",
		num_workers,
		num_workers > 1 ? "workers" : "worker");


	fprintf(stdout, "%f", production_time);

	printf(" sec(s)\nSpace Usage: %d    Deadlocks: %d\n", num_spaces, num_deadlocks);
	printf("=====================================\n");
	free(sem_value);
}

// initialize resource pack
void initResourcePack(struct resource_pack *pack,
		int space_limit, int num_workers) {
	initSem();
	pack->space_limit  = space_limit;
	pack->num_workers  = num_workers;
	pack->sem_space    = &sem_space   ;
	pack->sem_worker   = &sem_worker  ;

	pack->sem_skeleton = &sem_skeleton;
	pack->sem_engine   = &sem_engine  ;
	pack->sem_chassis  = &sem_chassis ;
	pack->sem_body     = &sem_body    ;

	pack->sem_window   = &sem_window  ;
	pack->sem_tire     = &sem_tire    ;
	pack->sem_battery  = &sem_battery ;
	pack->sem_car      = &sem_car     ;
}

// desotry semaphores
int destroySem(){
#ifdef DEBUG
	printf("Destroying semaphores...\n");
#endif
	sem_destroy(&sem_worker);
	sem_destroy(&sem_space);

	sem_destroy(&sem_skeleton);
	sem_destroy(&sem_engine);
	sem_destroy(&sem_chassis);
	sem_destroy(&sem_body);

	sem_destroy(&sem_window);
	sem_destroy(&sem_tire);
	sem_destroy(&sem_battery);
	sem_destroy(&sem_car);

	// sem_destroy(&sem_free_workers);
	sem_destroy(&sem_prod_workers);
	sem_destroy(&sem_assm_workers);
	sem_destroy(&sem_worker_status_register);
	sem_destroy(&sem_prod_trigger);
	sem_destroy(&sem_assm_trigger);
	sem_destroy(&sem_prod_cnt);
	sem_destroy(&sem_assm_cnt);
	sem_destroy(&sem_space_accrual);
#ifdef DEBUG
	printf("Semaphores destroyed\n");
#endif
	return 0;
}

// initialize semaphores
int initSem(){
#ifdef DEBUG
	printf("Initiating semaphores...\n");
#endif
	sem_init(&sem_worker,   0, num_workers);
	sem_init(&sem_space,    0, num_spaces);

	sem_init(&sem_skeleton, 0, 0);
	sem_init(&sem_engine,   0, 0);
	sem_init(&sem_chassis,  0, 0);
	sem_init(&sem_body,     0, 0);

	sem_init(&sem_window,   0, 0);
	sem_init(&sem_tire,     0, 0);
	sem_init(&sem_battery,  0, 0);
	sem_init(&sem_car,      0, 0);

	// sem_init(&sem_free_workers, 0,	num_workers);
	sem_init(&sem_prod_workers, 0,	num_prod_workers);
	sem_init(&sem_assm_workers, 0,	num_assm_workers);
	sem_init(&sem_worker_status_register, 0, 1);
	sem_init(&sem_prod_trigger, 0, 1);
	sem_init(&sem_assm_trigger, 0, 1);
	sem_init(&sem_prod_cnt, 0, 1);
	sem_init(&sem_assm_cnt, 0, 1);
	sem_init(&sem_space_accrual, 0, 1);

#ifdef DEBUG
	printf("Init semaphores done!\n");
#endif
	return 0;
}

void enable_production() {
    sem_wait(&sem_prod_trigger);
    prod_trigger = 1;
    sem_post(&sem_prod_trigger);
#ifdef DEBUG
    fprintf(stderr, "[Scheduler] Production line enabled\n");
#endif
}

void enable_assembly() {
    sem_wait(&sem_assm_trigger);
    assm_trigger = 1;
    sem_post(&sem_assm_trigger);
#ifdef DEBUG
    fprintf(stderr, "[Scheduler] Assembly line enabled\n");
#endif
}


