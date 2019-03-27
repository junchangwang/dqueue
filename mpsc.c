#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include "delay.h"
#include "queue.h"
#ifdef VERIFY
#include <sys/stat.h>  //mkdir
#include <sys/time.h>  //gettimeofday
#endif

#ifndef LOGN_OPS
#define LOGN_OPS 7
#endif

long nops;
static queue_t * q;
static handle_t ** hds;
#ifdef VERIFY
void** interm_enq[256];
void** interm_deq;
#endif
#ifdef WORKLOAD
extern int workload;
static inline uint64_t rdtsc_bare()
{
	uint64_t        time;
	uint32_t        msw, lsw;
	__asm__         __volatile__(
			"rdtsc\n\t"
			"mov %%edx, %0\n\t"
			"mov %%eax, %1\n\t"
			: "=r" (msw), "=r"(lsw)
			:
			: "%rax","%rdx");
	time = ((uint64_t) msw << 32) | lsw;
	return time;
}

static inline void wait_ticks(uint64_t ticks)
{
	uint64_t        current_time;
	uint64_t        time = rdtsc_bare();
	time += ticks;
	do {
		current_time = rdtsc_bare();
	} while (current_time < time);
}
#endif

void init(int nprocs, int logn) {
  /** Use 10^7 as default input size. */
  if (logn == 0) logn = LOGN_OPS;

  /** Compute the number of ops to perform. */
  nops = 1;
  int i;
  for (i = 0; i < logn; ++i) {
    nops *= 10;
  }

  printf("  Number of operations: %ld\n", nops);

  q = align_malloc(PAGE_SIZE, sizeof(queue_t));
  queue_init(q, nprocs);
  hds = align_malloc(PAGE_SIZE, sizeof(handle_t * [nprocs]));

#ifdef VERIFY
  interm_deq = (void **) calloc(nops, sizeof(void *));
    if( interm_deq == NULL ) {
      perror("Failed in allocating intermediate array for recording values dequeued.");
    }
  for(i = 1; i < nprocs; ++i) {
    interm_enq[i] = (void **) calloc(nops/(nprocs - 1), sizeof(void *));
    if( interm_enq[i] == NULL ) {
      perror("Failed in allocating intermediate array for recording values enqueued.");
    }
  }
#endif

  printf("  Working in Single-Producer-Multi-Consumers module.\n");
}

void thread_init(int id, int nprocs) {
  hds[id] = align_malloc(PAGE_SIZE, sizeof(handle_t));

  static pthread_mutex_t mutex;
  pthread_mutex_lock(&mutex);
  queue_register(q, hds[id], id);
  pthread_mutex_unlock(&mutex);
}

void thread_exit(int id, int nprocs) {
  queue_free(q, hds[id]);
}

void * benchmark(int id, int nprocs) {
  void * val = (void *) (intptr_t) (id + 1);
  handle_t * th = hds[id];

  delay_t state;
  delay_init(&state, id);

  if (id == 0) { // Consumer
    for (unsigned long i = 0; i < (nops/(nprocs - 1))*(nprocs - 1); i++) {
      val = dequeue(q, th);
#ifdef WORKLOAD
      wait_ticks((uint64_t)(workload * 2.6));
#endif
#ifdef VERIFY
      interm_deq[i] = val;
#endif
    }
  }
  else {
    unsigned long base_i = (id - 1) * (nops/(nprocs - 1));
    for (unsigned long i = 0; i < (nops/(nprocs - 1)); ++i) {
      enqueue(q, th, (void *)(intptr_t)(base_i + i));
#ifdef WORKLOAD
      /* The CPU on the testbed is running at 2.6GHz. */
      wait_ticks((uint64_t)(workload * 2.6)*(nprocs-1));
#endif
#ifdef VERIFY
      interm_enq[id][i] = (void *)(intptr_t)(base_i + i);
#endif
    }
  }

  return val;
}

int verify(int nprocs, void ** results) {
#ifdef VERIFY
  struct timeval t;
  char dir_name[80];
  gettimeofday(&t, NULL);
  sprintf(dir_name, "log-%lu", (unsigned long)(t.tv_sec * 1000000 + t.tv_usec));
  if (mkdir(dir_name, 0775) != 0) {
    perror("Verify: ");
    exit(-1);
  }

  char file_name[100];
  sprintf(file_name, "%s/%s", dir_name, "consumer");
  FILE* fp = fopen(file_name, "w");
  if (fp == NULL) {
    perror("Open Consumer log file: ");
    exit(-1);
  }
  for (long i = 0; i < (nops/(nprocs - 1))*(nprocs - 1); ++i) {
    fprintf(fp, "%lu\t\t", (unsigned long)interm_deq[i]);
    if (i%5 == 4)
      fprintf(fp, "\n");
  }
  fclose(fp);

  for (int t = 1; t < nprocs; ++t) {
    sprintf(file_name, "%s/%s-%d", dir_name, "producer", t);
    fp = fopen(file_name, "w");
    if (fp == NULL) {
      perror("Open Producer log file: ");
      exit(-1);
    }
    for (long i = 0; i < nops / (nprocs-1); ++i) {
      fprintf(fp, "%lu\t\t", (unsigned long)interm_enq[t][i]);
      if (i%5 == 4)
        fprintf(fp, "\n");
    }
    fclose(fp);
  }
  return 0;
#else
  return 0;
#endif
}
