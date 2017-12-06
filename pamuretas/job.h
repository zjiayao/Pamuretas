#include "definitions.h"

void makeSkeleton(sem_t *sem_space, sem_t *sem_skeleton);
void makeEngine(sem_t *sem_space, sem_t *sem_engine);
void makeChassis(sem_t *sem_space, sem_t *sem_chassis);
void makeWindow(sem_t *sem_space, sem_t *sem_window);
void makeTire(sem_t *sem_space, sem_t *sem_tire);
void makeBattery(sem_t *sem_space, sem_t *sem_battery );
void makeBody(sem_t *sem_space, int space_limit, sem_t *sem_body,
		sem_t *sem_skeleton, sem_t *sem_engine, sem_t *sem_chassis);
void makeCar(sem_t *sem_space, int space_limit, sem_t *sem_car,
		sem_t *sem_window, sem_t *sem_tire, sem_t *sem_battery, sem_t *sem_body);


int requestSpace(sem_t *space);
void releaseSpace(sem_t *space, int space_limit);
void makeItem(sem_t *space, int makeTime, sem_t* item);
void getItem(sem_t *space, int space_limit, sem_t *item);
void reportJobDone(sem_t *sem_worker, int num_worker);
