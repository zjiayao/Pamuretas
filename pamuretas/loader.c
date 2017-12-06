#define BENCH
#include "main.c"

#define I 1
#define J 27
#define K 24

typedef struct triple {
    int i, j, k, c, w, s;
} triple;
int CARS[I] = {1};
int WORKERS[J] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 50, 64, 72, 86, 100, 120};
int SPACES[K] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 40, 30, 50, 100};
pthread_t threads[I * J * K];
double arr[I * K * J] = {0};
struct triple tps[I * K * J];
inline int idx(int i, int j, int k)
{ return k * (I * J) + j * I + i; }

void* report(void* arg) {
    triple *trp = (triple*) arg;
    if(!fork()) {
	// usleep(idx(trp->i, trp->j, trp->k) * 15 * 1000000);
	char buffer[128];
	sprintf(buffer, "logs/%02d-%03d-%03d.log", trp->c, trp->w, trp->s);
	// fprintf(stdout, "%s\n", buffer);
	// FILE *flog = fopen(buffer, "w");
	double t = bench(trp->c, trp->w ,trp->s, buffer);
	fprintf(stdout, "%d Car(s), %d Worker(s), %d Spaces (s): %f\n", trp->c, trp->w, trp->s, t);
	//fclose(flog);
	exit(0);
    }

    pthread_exit(NULL);
}
int main(int argc, char** argv) {
    int i = 0;
    assert(argc == 2 || argc == 4);
    if(argc == 2) {
	printf("Batch processes mode\n");
	CARS[i] = atoi(argv[1]);
	for(int j = 0; j < J; ++j) {
	    for (int k = 0; k < K; ++k) {
		tps[idx(i, j, k)].i = i;
		tps[idx(i, j, k)].j = j;
		tps[idx(i, j, k)].k = k;
		tps[idx(i, j, k)].c = CARS[i];
		tps[idx(i, j, k)].w = WORKERS[j];
		tps[idx(i, j, k)].s = SPACES[k];
		register int rc;
		rc = pthread_create(&threads[idx(i, j, k)], NULL, report, &tps[idx(i, j, k)]);
		if (rc) {
		    fprintf(stderr, "ERROR CREATING THREAD\n");
		    return EXIT_FAILURE;
		}
	    }
	}
	for (int j = 0; j < I * J * K; ++j)
	{
	    pthread_join(threads[j], NULL);
	}
    } else if (argc == 4) {
	printf("Single process mode\n");
	tps[0].i = tps[0].j = tps[0].k = 0;
	tps[0].c = atoi(argv[1]);
	tps[0].w = atoi(argv[2]);
	tps[0].s = atoi(argv[3]);
	register int rc;
	pthread_t thread;

	rc = pthread_create(&thread, NULL, report, &tps[0]);
	if(rc) {
		fprintf(stderr, "ERROR CREATING THREAD\n");
		return EXIT_FAILURE;
	}
	pthread_join(thread, NULL);


    }
    return EXIT_SUCCESS;
}

