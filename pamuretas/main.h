#include "definitions.h"
#include "worker.h"
int initSem();
int destroySem();
void initResourcePack(struct resource_pack *pack,
		int space_limit, int num_workers);
void reportResults(double);
void* group_produce(void* arg);
