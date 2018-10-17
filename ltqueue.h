#ifndef LTQUEUE_H
#define LTQUEUE_H

#ifdef LTQUEUE
#include "align.h"

typedef struct _node_t {
  struct _node_t * volatile next DOUBLE_CACHE_ALIGNED;
  void * data DOUBLE_CACHE_ALIGNED;
  unsigned long timestamp;
} node_t DOUBLE_CACHE_ALIGNED;

typedef struct _queue_t {
  struct _handle_t * producers;
} queue_t DOUBLE_CACHE_ALIGNED;

typedef struct _handle_t {
  struct _handle_t * next;
  unsigned long volatile size;
  struct _node_t * volatile head DOUBLE_CACHE_ALIGNED;
  struct _node_t * volatile tail DOUBLE_CACHE_ALIGNED;
} handle_t DOUBLE_CACHE_ALIGNED;

#endif

#endif /* end of include guard: LTQUEUE_H */
