#include "definitions.h"
#include "job.h"
#include <pthread.h>

void return_free_worker(int idx, int gid);

typedef struct resource_pack {
	int space_limit;
	int num_workers;
	sem_t *sem_space;
	sem_t *sem_worker;

	sem_t *sem_skeleton;
	sem_t *sem_engine;
	sem_t *sem_chassis;
	sem_t *sem_body;

	sem_t *sem_window;
	sem_t *sem_tire;
	sem_t *sem_battery;
	sem_t *sem_car;
} resource_pack;

typedef struct work_pack {
	int tid;   // worker ID, 8bytes
	int jid;   // job ID, 8bytes
	char gid;   // group ID, 2bytes
	char status; // 2bytes
	short times; // how many times the job be run, 4bytes
	resource_pack *resource; // 8bytes
#if __SIZEOF_POINTER__ == 8
#else
	char pad;
#endif
} work_pack; // 32bytes

typedef struct work_queue {
    int begin, end;
    work_pack* queue;
} work_queue;

void *work(void *arg);


