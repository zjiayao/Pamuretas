#include "job.h"

void reportJobDone(sem_t *sem_worker, int num_worker) {
	int *num_free_worker = (int*) malloc(sizeof(num_free_worker));
	sem_getvalue(sem_worker, num_free_worker);
	if(*num_free_worker < num_worker) {
		free(num_free_worker);
		sem_post(sem_worker);
	}else {
		printf("Error, number of free workers exceeds num_worker\n");
		free(num_free_worker);
		exit(1);
	}
}

int requestSpace(sem_t *space) {
#ifdef DEBUG
	int *num_free_space = (int*) malloc(sizeof(num_free_space));
	sem_getvalue(space, num_free_space);
	printf("Requesting free space, current space=%d...\n", *num_free_space);
#endif
	int acquired = 0;
	while(!acquired) {
	    if(sem_trywait(space) == 0) {
		// acqiored
		acquired = 1;
	    } else {
		usleep(50);
	    }
	}

#ifdef DEBUG
	sem_getvalue(space, num_free_space);
	printf("Space requested, current space=%d...\n", *num_free_space);
#endif

	return 0;
}

void releaseSpace(sem_t *space, int space_limit) {
	int *num_free_space = (int*) malloc(sizeof(num_free_space));
	sem_getvalue(space, num_free_space);
	if(*num_free_space < space_limit) {
#ifdef DEBUG
		printf("releasing free space, current space=%d...\n", *num_free_space);
#endif
		sem_post(space);
#ifdef DEBUG
		sem_getvalue(space, num_free_space);
		printf("Space released, current space=%d...\n", *num_free_space);
#endif
		free(num_free_space);
	} else {
		printf("Error, releasing space that doesn't exist\n");
		free(num_free_space);
		exit(1);
	}
}

void makeItem(sem_t *space, int makeTime, sem_t* item) {
	sleep(makeTime);
	requestSpace(space);
	sem_post(item);
}

void getItem(sem_t *space, int space_limit, sem_t *item) {
	sem_wait(item);
	releaseSpace(space, space_limit);
}

void makeSkeleton(sem_t *sem_space, sem_t *sem_skeleton) {
	makeItem(sem_space, TIME_SKELETON, sem_skeleton);
}

void makeEngine(sem_t *sem_space, sem_t *sem_engine) {
	makeItem(sem_space, TIME_ENGINE, sem_engine);
}

void makeChassis(sem_t *sem_space, sem_t *sem_chassis) {
	makeItem(sem_space, TIME_CHASSIS, sem_chassis);
}

void makeWindow(sem_t *sem_space, sem_t *sem_window) {
	makeItem(sem_space, TIME_WINDOW, sem_window);
}

void makeTire(sem_t *sem_space, sem_t *sem_tire) {
	makeItem(sem_space, TIME_TIRE, sem_tire);
}

void makeBattery(sem_t *sem_space, sem_t *sem_battery ) {
	// call makeItem and pass in the time for makeing battery
	makeItem(sem_space, TIME_BATTERY, sem_battery);
}

void makeBody(sem_t *sem_space, int space_limit, sem_t *sem_body,
		sem_t *sem_skeleton, sem_t *sem_engine, sem_t *sem_chassis) {
	int acquired = 0, n_skeleton = 0, n_engine = 0, n_chassis = 0;

	while(!acquired) {
	    acquired = 1;

	    if(n_skeleton < 1) {
		if(sem_trywait(sem_skeleton) == 0) {
		    releaseSpace(sem_space, space_limit);
		    ++n_skeleton;
		} else {
		    acquired = 0;
		}
	    }

	    if(n_engine < 1) {
		if(sem_trywait(sem_engine) == 0) {
		    releaseSpace(sem_space, space_limit);
		    ++n_engine;
		} else {
		    acquired = 0;
		}
	    }

	    if(n_chassis < 1) {
		if(sem_trywait(sem_chassis) == 0) {
		    releaseSpace(sem_space, space_limit);
		    ++n_chassis;
		} else {
		    acquired = 0;
		}
	    }
	    if(acquired) { break; }
	    usleep(500);
#ifdef DEBUG
	    fprintf(stderr, "[Body Worker] Requests:\n\tSkeleton: %d    Engine: %d\n\tChassis: %d\n", n_skeleton, n_engine, n_chassis);
#endif
	}

	makeItem(sem_space, TIME_BODY, sem_body);
}

void makeCar(sem_t *sem_space, int space_limit, sem_t *sem_car,
		sem_t *sem_window, sem_t *sem_tire, sem_t *sem_battery, sem_t *sem_body) {
	int acquired = 0, n_body = 0, n_windows = 0, n_tires = 0, n_battery = 0;

	while(!acquired) {
	    acquired = 1;
	    if(n_body < 1) {
		if(sem_trywait(sem_body) == 0) {
		    releaseSpace(sem_space, space_limit);
		    ++n_body;
		} else {
		    acquired = 0;
		}
	    }

	    if(n_windows < 7) {
		if(sem_trywait(sem_window) == 0) {
		    releaseSpace(sem_space, space_limit);
		    if(++n_windows < 7)
		    { acquired = 0; }
		} else {
		    acquired = 0;
		}
	    }

	    if(n_tires < 4) {
		if(sem_trywait(sem_tire) == 0) {
		    releaseSpace(sem_space, space_limit);
		    if(++n_tires < 4)
		    { acquired = 0; }
		} else {
		    acquired = 0;
		}
	    }

	    if(n_battery < 1) {
		if(sem_trywait(sem_battery) == 0) {
		    releaseSpace(sem_space, space_limit);
		    ++n_battery;
		} else {
		    acquired = 0;
		}
	    }

	    if(acquired) { break; }
	    usleep(500);
#ifdef DEBUG
	    fprintf(stderr, "[Car Worker] Requests:\n\tBody: %d    Window: %d\n\tTire: %d    Battery: %d\n", n_body, n_windows, n_tires, n_battery);
#endif
	}

	// assembly car by sleep ^_^
	sleep(TIME_CAR);
	// notify the completion of making a car by post to sem_car
	sem_post(sem_car);
}

