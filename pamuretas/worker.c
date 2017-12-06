#include "worker.h"

void *work(void *arg) {
    work_pack *wpack = (work_pack *)arg;
    int gid = wpack->gid;
    int tid = wpack->tid;
    int jid = wpack->jid;
    int times = wpack->times;
    resource_pack *pack = wpack->resource;
    int space_limit = pack->space_limit;
    sem_t *sem_space    = pack->sem_space;
    sem_t *sem_skeleton = pack->sem_skeleton;
    sem_t *sem_engine   = pack->sem_engine;
    sem_t *sem_chassis  = pack->sem_chassis;
    sem_t *sem_body     = pack->sem_body;
    sem_t *sem_window   = pack->sem_window;
    sem_t *sem_tire     = pack->sem_tire;
    sem_t *sem_battery  = pack->sem_battery;
    sem_t *sem_car      = pack->sem_car;

    // int num_workers = pack->num_workers;
    sem_t *sem_worker   = pack->sem_worker;
    sem_wait(sem_worker);
#ifdef DEBUG
    int *current_num_workers = malloc(sizeof(int));
    sem_getvalue(sem_worker, current_num_workers);
    printf("[Worker(%d/%d)]: working on job %d for %d %s...\n",
		    tid, *current_num_workers, jid, times, times > 1 ? "times" : "time");
    free(current_num_workers);
#endif
    int i;
    for (i = 0; i < times; i++)
    {
	switch(jid) {
	    case SKELETON  :
		makeSkeleton(sem_space, sem_skeleton);
		#ifdef DEBUG
		printf("[Worker(%d)]: One car skeleton made!\n", tid);
		#endif
		break;
	    case ENGINE    :
		makeEngine(sem_space, sem_engine);
		#ifdef DEBUG
		printf("[Worker(%d)]: One engine made!\n", tid);
		#endif
		break;
	    case CHASSIS   :
		makeChassis(sem_space, sem_chassis);
		#ifdef DEBUG
		printf("[Worker(%d)]: One chassis made!\n", tid);
		#endif
		break;
	    case BODY      :
		makeBody(sem_space, space_limit, sem_body,
				sem_skeleton, sem_engine, sem_chassis);
		#ifdef DEBUG
		printf("[Worker(%d)]: One body made!\n", tid);
		#endif
		break;
	    case WINDOW    :
		makeWindow(sem_space, sem_window);
		#ifdef DEBUG
		printf("[Worker(%d)]: One window made!\n", tid);
		#endif
		break;
	    case TIRE      :
		makeTire(sem_space, sem_tire);
		#ifdef DEBUG
		printf("[Worker(%d)]: One tire made!\n", tid);
		#endif
		break;
	    case BATTERY   :
		makeBattery(sem_space, sem_battery );
		#ifdef DEBUG
		printf("[Worker(%d)]: One battery pack made!\n", tid);
		#endif
		break;
	    case CAR       :
		makeCar(sem_space, space_limit, sem_car,
				sem_window, sem_tire, sem_battery, sem_body);
		#ifdef DEBUG
		printf("[Worker(%d)]: One car made!\n", tid);
		#endif
		break;
	    default:
		printf("[Worker(%d)]: No such kind of job\n", tid);
		break;
	}
    }
#ifdef DEBUG
    printf("[Worker(%d)]: job %d done!\n", tid, jid);
#endif
    wpack->status = JOB_FINISHED;
    sem_post(sem_worker);
    return_free_worker(tid, gid);
    pthread_exit(NULL);
}



