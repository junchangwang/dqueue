// void enqueue(queue_t * q, handle_t * handle, void * data)
// {
//   // if (NEXT(q->tail) == q->head)
//   //   return;
//   // q->buffer[q->tail] = data;
//   // q->tail = NEXT(q->tail);

//   unsigned long pos = FAA(&q->local_tail, 1);
//     q->buffer[pos] = data;
//     FENCE();
//   if (q->tail == pos ) {
//     int fast_forward = 1;
//     for (int i = 1; i < BATCH_SIZE; ++i) {
//       if (READ_ONCE(q->buffer[pos + i]) != BOTTOM)
//         fast_forward ++;
//       else
//         break;
//     }
//     FAA(&q->tail, fast_forward);
//     FENCE();
//   }
// }

// First working version
// void enqueue(queue_t * q, handle_t * handle, void * data)
// {
//   unsigned long pos = FAA(&q->local_tail, 1);
//   q->buffer[pos] = data;
//   FENCE();
//   while (READ_ONCE(q->tail) != pos) {};
//   FAA(&q->tail, 1);
// }

// void * dequeue(queue_t * q, handle_t * handle)
// {
//   if (q->head == q->tail) {
//     while (READ_ONCE(q->head) == READ_ONCE(q->tail)) {};
//   }
//   void * tmp = q->buffer[q->head];
//   q->head = NEXT(q->head);
//   return tmp;
// }

static inline uint64_t rdtsc_bare()
{
  uint64_t  time;
  uint32_t  msw, lsw;
  __asm__   __volatile__(
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
  uint64_t  current_time;
  uint64_t  time = rdtsc_bare();
  time += ticks;
  do {
    current_time = rdtsc_bare();
  } while (current_time < time);
}