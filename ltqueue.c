#include <unistd.h>
#include <stdlib.h>
#include "delay.h"
#include "ltqueue.h"
#include "primitives.h"

static unsigned long timestamp;
queue_t * q_g;
#define MAX_BUFFER_SIZE (4096)

void queue_init(queue_t * q, int nprocs)
{
  timestamp = 1;
  q->producers = NULL;
  q_g = q;
}

void queue_register(queue_t * q, handle_t * th, int id)
{
  th->size = 0;
  node_t * anchor = (node_t *)malloc(sizeof(node_t));
    if (anchor == NULL) {
    printf("Error in allocating node.\n");
    return;
  }
  anchor->data = NULL;
  anchor->timestamp = 0;
  th->head = th->tail = anchor;

  if (id >= 1) { // Producers
    th->next = q->producers;
    q->producers = th;
  }
}

void enqueue(queue_t * q, handle_t * handle, void * data)
{
  node_t * node = (node_t *)malloc(sizeof(node_t));
  if (node == NULL) {
    printf("Error in allocating node.\n");
    return;
  }
  node->data = data;
  node->timestamp = FAA(&timestamp, 1);
  node->next = NULL;
  handle->tail->next = node;
  handle->tail = node;
  FAA(&handle->size, 1);
}

void * dequeue(queue_t * q, handle_t * handle)
{
  handle_t * target = q->producers;
  unsigned long timestamp = (unsigned long)(-1);
recheck:
  for(handle_t * tmp = q->producers; tmp != NULL; tmp=tmp->next) {
    if (tmp->head == tmp->tail)
      break;

    if (tmp->head->next->timestamp < timestamp) {
      timestamp = tmp->head->next->timestamp;
      target = tmp;
    }
  }
  if (timestamp == (unsigned long)(-1))
    goto recheck;
  if (target->head->next == target->tail) 
      return target->head->next->data;
  node_t * node = target->head->next;
  void * val = node->data;
  target->head->next = target->head->next->next;
  free(node);
  __atomic_fetch_sub(&target->size, 1, __ATOMIC_RELAXED);
  return val;
}

void queue_free(int id, int nprocs) {}
