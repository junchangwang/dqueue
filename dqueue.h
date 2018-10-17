#ifndef DQUEUE_H
#define DQUEUE_H

#ifdef DQUEUE
#include "align.h"

#define DATA_TYPE void*   /* E.g., void * or long */
#define BOT (DATA_TYPE)(intptr_t)(-1)
#define POS_BOT (-1)
#define LOCAL_BUFFER_SIZE (16)
#define DQUEUE_SEG_SIZE ((1UL << 10) - 2)

typedef struct {
  DATA_TYPE value;
  long enqueue_pos;
} buffer_elem;

typedef struct __segment {
  struct __segment * volatile next;
  long volatile id;
  DATA_TYPE queue_data[DQUEUE_SEG_SIZE];
} Segment DOUBLE_CACHE_ALIGNED;

typedef struct {
  unsigned long volatile head DOUBLE_CACHE_ALIGNED;
  unsigned long volatile tail DOUBLE_CACHE_ALIGNED;
  Segment * HSeg;
} queue_t DOUBLE_CACHE_ALIGNED;

typedef struct __handle_t {
  struct __handle_t * next;
  buffer_elem * local_buffer;
  long local_head;
  long local_tail;
  Segment * volatile HSeg;
} handle_t DOUBLE_CACHE_ALIGNED;

/* To integrate into Chaoran's framework, consumer reuses handle_t, even though
** the consumer only uses field CSeg.    */
// typedef struct __handle_consumer {
//   Segment * volatile CSeg;
// } handle_c DOUBLE_CACHE_ALIGNED;

#endif

#endif /* end of include guard: DQUEUE_H */
