#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <time.h>

struct timespec start, end;
pthread_t* thread = NULL;
long long numThreads = 1;
long long numIterations = 1;
int opt_yield = 0;
char sync = '.';
int lock = 0;
char testName[20] ="add";
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;


void printError(char* a){
	int err = errno;
	fprintf(stderr, strerror(err));
	fprintf(stderr, a);
}

void getLock(){
	switch(sync){
		case 'm':
			if(pthread_mutex_lock(&mut)){
				printError("Fail to lock.\n");
				exit(1);
			}
			break;
		case 's':
			while( __sync_lock_test_and_set(&lock,1));
			break;
		default:
			exit(1);
			break;
	}
}

void releaseLock(){
	switch(sync){
		case 'm':
			if(pthread_mutex_unlock(&mut)){
				printError("Fail to lock.\n");
				exit(1);
			}
			break;
		case 's':
			__sync_lock_release(&lock);
			break;
		default:
			exit(1);
			break;
	}
}


void add(long long *pointer, long long value) {
		long long sum = *pointer + value;
		if (opt_yield)
				sched_yield();
		*pointer = sum;
}

void addCAS(long long* ptr, long long value){
	long long old = *ptr;
	while(__sync_val_compare_and_swap(ptr,old,old+value)!= old){
		if (opt_yield)
				sched_yield();
		old = *ptr;
	}
}
void* threadFunc(void* ptr) {
	long long i =0;
	for (i =0; i<numIterations; i++){
		if (sync == 'c'){
			addCAS((long long*)ptr,1);
			addCAS((long long*)ptr,-1);
		}
		else if(sync != '.') {
			getLock();
			add((long long *)ptr,1);
			releaseLock();
			getLock();
			add((long long *)ptr,-1);
			releaseLock();
		}
		else {
			add((long long *)ptr,1);
			add((long long *)ptr,-1);
		}
		
	}
	return NULL;
}

void out(){
	if(thread!=NULL){
		free(thread);
	}
}


int main(int argc, char* argv[]){
	int i;
	atexit(out);
	
	int opt;
	static struct option long_opts[] = {
		{"threads", required_argument, NULL, 't'},
		{"iterations", required_argument, NULL, 'i'},
		{"yield", no_argument, NULL, 'y'},
		{"sync", required_argument, NULL, 's'},
		{0, 0, 0, 0}
	};
	while( (opt = getopt_long(argc, argv, "t:i:ys:", long_opts, NULL)) != -1){
		switch(opt){
		case 't':
			numThreads = atoi(optarg);
			break;
		case 'i':
			numIterations = atoi(optarg);
			break;
		case 'y':
			opt_yield = 1;
			break;
		case 's':
			if(strlen(optarg) >1){
				fprintf(stderr, "Wrong argument format. Usage: %s [--threads=<numThreads> --iterations=<numIteration>] \n",argv[0]);
				exit(1);
			}
			sync = optarg[0];
			break;
		case '?':
			fprintf(stderr, "Wrong argument format. Usage: %s [--threads=<numThreads> --iterations=<numIteration>] \n",argv[0]);
			exit(1);
		default:
			break;
		}
	}
	
	if(opt_yield){
		strcat(testName, "-yield");
	}
	switch(sync){
		case '.':
			strcat(testName, "-none");
			break;
		case 'm':
			strcat(testName, "-m");
			break;
		case 's':
			strcat(testName, "-s");
			break;
		case 'c':
			strcat(testName, "-c");
			break;
		default:
			fprintf(stderr, "Wrong argument format. Usage: %s [--threads=<numThreads> --iterations=<numIteration> --yield=m/s/c] \n",argv[0]);
			exit(1);
	}
	
	
	
	
	thread = (pthread_t*)malloc(sizeof(pthread_t)*numThreads);
	long long counter = 0;
	
	if(clock_gettime(CLOCK_MONOTONIC, &start)){
		printError("Fail to get clock start\n");
		exit(1);
	}
	
	for(i=0;i<numThreads;i++){
		if(pthread_create((thread+i),NULL,threadFunc,&counter)){
			printError("Fail to create threads.\n");
			exit(1);
		}
	}
	
	for(i=0;i<numThreads;i++){
		if(pthread_join(thread[i],NULL)){
			printError("Fail to join threads.\n");
			exit(1);
		}
	}
	
	if(clock_gettime(CLOCK_MONOTONIC, &end)){
		printError("Fail to get clock end\n");
		exit(1);
	}
	long long ellapse = end.tv_nsec - start.tv_nsec;
	long long numOp = numThreads*numIterations*2;
	fprintf(stdout,"%s,%lld,%lld,%lld,%lld,%lld,%lld\n",testName,numThreads,numIterations,
		numOp,ellapse,ellapse/numOp,counter);
	
	return 0;
}