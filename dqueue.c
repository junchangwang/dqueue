#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "delay.h"
#include "dqueue.h"
#include "primitives.h"

extern long nops;
static handle_t * producers;
static handle_t * consumer;
static queue_t * queue_g;
static long volatile gc_goflag;

static Segment * new_segment() {
	Segment *s = align_malloc(PAGE_SIZE, sizeof(Segment));
	s->next = NULL;
	s->id = 0;
	for (long i = 0; i < DQUEUE_SEG_SIZE; ++i)
		s->queue_data[i] = BOT;
	return s;
}

static void * gc(void *);
static pthread_t th_gc;
static void * res;

void queue_init(queue_t * q, int nprocs)
{
	q->head = q->tail = 0;
	q->HSeg = new_segment();
	if (q->HSeg == NULL)
		perror("Allocating queue segment failed.");
	queue_g = q;
	gc_goflag = 1;
	pthread_create(&th_gc, NULL, gc, NULL);
}

void queue_register(queue_t * q, handle_t * th, int id)
{
	th->next = NULL;

	if (id >= 1) { // For producers only
		th->local_buffer = (buffer_elem *) malloc(LOCAL_BUFFER_SIZE * sizeof(buffer_elem));
		if (th->local_buffer == NULL) 
			perror("Allocating local buffer: ");
		for (int i = 0; i < LOCAL_BUFFER_SIZE; i++) {
			th->local_buffer[i].enqueue_pos = POS_BOT;
			th->local_buffer[i].value = BOT;
		}
		th->local_head = th->local_tail = 0;
		th->HSeg = q->HSeg;

		// To organize handlers of producers as a linked list.
		// mpsc.c uses a mutex lock to protect the concurrent accesses to global variable *producers*.
		if (producers == NULL) {
			producers = th;
		}
		else {
			handle_t * tmp = producers;
			while (tmp->next != NULL) {
				tmp = tmp->next;
			}
			tmp->next = th;
		}
	}
	else {  // For consumer
		th->local_buffer = NULL;
		th->local_head = th->local_tail = 0;
		th->HSeg = q->HSeg;
		th->next = NULL;
		consumer = th;
	}
}

unsigned long int NEXT(unsigned long x, unsigned long size)
{
	int tmp = x + 1;
	if (tmp >= size)
		return 0;
	else
		return tmp;
}

unsigned long int WRAP(unsigned long x, unsigned long size)
{
	return (x % size);
}

Segment * find_segment(Segment *start, long id, handle_t *th) 
{
	Segment * curr = start;
	Segment * next;
	for (long i = curr->id; i < id / DQUEUE_SEG_SIZE; i++) {
		next = curr->next;
		if (next == NULL) {
			Segment * new = new_segment();
			new->id = i + 1;
			if (CASra(&curr->next, &next, new)) {
				next = new;
				//printf("Successfully allocate segment (ID: %ld)\n", new->id);
			}
			else {
				free(new);
			}
		}
		curr = next;
	}
	return curr;
}

void dump_local_buffer(queue_t * q, handle_t * th) 
{
	long head, tmp_pos;
	DATA_TYPE tmp_value;
	Segment * seg;
	/* This thread (producer) is the only one who can update variables *local_head* and *local_tail* */
	while (th->local_head != th->local_tail) {
		head = th->local_head;
		tmp_pos = th->local_buffer[head].enqueue_pos;
		tmp_value = th->local_buffer[head].value;
		seg = find_segment(th->HSeg, tmp_pos, th);
		th->HSeg = seg;
		DATA_TYPE * cell = &(seg->queue_data[tmp_pos % DQUEUE_SEG_SIZE]);
		*cell = tmp_value;
		//NOTE: Need barrier for architectures other than x86. 
		//      Pair with barrier in help_enqueue().
		th->local_head = NEXT(th->local_head, LOCAL_BUFFER_SIZE);
	}
}

void help_enqueue(queue_t * q) 
{
	handle_t * tmp_handle = producers;
	while (tmp_handle != NULL) {
		/* Since an enqueue request is two 64-bit word that aren't read or written atomically,
		 ** when an enqueue helper reads the two words, it must identify if both belong to the
		 ** same logical request.
		 ** A helper reads an enqueue request's two words in the reverse order they were written,
		 ** such that the data read belongs to request *tmp_pos* or a later request.
		 ** If the data read is for a later request, a value has been written into cell *tmp_pos*,
		 ** and the program will notice this and return by checking if *enqueued_value ==BOT*
		 ** */
		long tmp_head = tmp_handle->local_head;
		long tmp_tail = tmp_handle->local_tail;
		//NOTE: Need read barrier for architectures other than x86. 
		//      Pair with barrier in enqueue().
		for (int i = 0; (i < LOCAL_BUFFER_SIZE) && 
				(WRAP(tmp_head + i, LOCAL_BUFFER_SIZE) != tmp_tail); 
				i++) {
			long head = WRAP(tmp_head+i, LOCAL_BUFFER_SIZE);
			long tmp_pos = tmp_handle->local_buffer[head].enqueue_pos;
			DATA_TYPE tmp_value = tmp_handle->local_buffer[head].value;
			Segment * seg = find_segment(tmp_handle->HSeg, tmp_pos, tmp_handle);
			if (seg->id > tmp_pos / DQUEUE_SEG_SIZE) {
				/* Corresponding producer is making progress.   **
				 ** Ther is no need to help it.                  */
				break;
			}
			DATA_TYPE * cell = &(seg->queue_data[tmp_pos % DQUEUE_SEG_SIZE]);
			if ( *cell == BOT ) {
				*cell = tmp_value;
			}
		}
		tmp_handle = tmp_handle->next;
	}
}

void enqueue(queue_t * q, handle_t * handle, void * value)
{
	while (NEXT(handle->local_tail, LOCAL_BUFFER_SIZE) == handle->local_head) {
		dump_local_buffer(q, handle);
	}
	long tail = handle->local_tail;
	handle->local_buffer[tail].value = value;
	handle->local_buffer[tail].enqueue_pos = FAA(&q->tail, 1);
	//NOTE: Need write barrier for architectures other than x86
	handle->local_tail = NEXT(handle->local_tail, LOCAL_BUFFER_SIZE);
}

void * dequeue(queue_t * q, handle_t * th)
{
	//DATA_TYPE volatile * cell = find_cell(&th->Hqs, q->head, th);
	Segment * seg = find_segment(th->HSeg, q->head, th);
	th->HSeg = seg;
	DATA_TYPE * cell = &(seg->queue_data[q->head % DQUEUE_SEG_SIZE]);
	while (*cell == BOT) {
		if (q->head == q->tail) {
			//NOTE: or return FULL.
			usleep(10);
		}
		else {
			usleep(10);
			help_enqueue(q);
		}
	}
	DATA_TYPE tmp = *cell;
	q->head ++;
	return tmp;
}

void queue_free(int id, int nprocs)
{
	static long lock = 0;

	if (FAA(&lock, 1) == 0) {
		gc_goflag = 0;
		pthread_join(th_gc, &res);
		while(queue_g->HSeg != NULL) {
			Segment * tmp = queue_g->HSeg;
			queue_g->HSeg = queue_g->HSeg->next;
			//printf("Clean up segment (ID: %ld) to shutdown.\n", tmp->id);
			free(tmp);
		}
	}
}

static void * gc(void * var)
{
	queue_t * q;
	long min_id, guard_id, max_id;

	while (gc_goflag) {
		q = queue_g;

		// To bypass the situation where the system just starts.
		if (!q || !consumer || !consumer->HSeg)
			goto Sleep;

		min_id = q->HSeg->id;
		max_id = consumer->HSeg->id;
		guard_id = max_id;
		if (min_id >= max_id)
			goto Sleep;

		for (handle_t * tmp = producers; tmp != NULL; tmp=tmp->next) {
			int seg_id = tmp->HSeg->id;
			if (seg_id < guard_id) {
				guard_id = seg_id;
			}
		}
		if (min_id >= guard_id)
			goto Sleep;

		// Garbage Collection for sublist [min_id, guard_id)
		for (int i = 0; i < (guard_id - min_id); i++) {
			Segment * tobe_freed = q->HSeg;
			q->HSeg = q->HSeg->next;
			//printf("To free retired segment (ID: %ld)\n", tobe_freed->id);
			free(tobe_freed);
		}

		/* Garbage Collection for sublist [gard_id, max_id), which is subtle because  while the GC thread is reclaiming segments in this sublist, producer threads may access them concurrently. */

		// // Allocate buffer to record segments which producer threads are accessing.
		// int id_buffer_len = max_id - min_id;
		// long * id_buffer = malloc(id_buffer_len * sizeof(long));
		// for (int i = 0; i < id_buffer_len; i++)
		//   id_buffer[i] = -1;
		// int id_buffer_index = 0;

		// for (handle_t * tmp = producers; tmp != NULL; tmp=tmp->next) {
		//   for (int i = 0; i < LOCAL_BUFFER_SIZE; i++) {
		//     int seg_id = (tmp->local_buffer[i].enqueue_pos / DQUEUE_SEG_SIZE);
		//     if ( seg_id < guard_id) {
		//       guard_id = seg_id;
		//     }
		//     else if (seg_id < max_id) {
		//       int found = 0;
		//       for (int j = 0; j < id_buffer_len; j++) {
		//         if (id_buffer[j] == seg_id) {
		//           found = 1;
		//           break;
		//         }
		//       }
		//       if (found == 0)
		//         id_buffer[id_buffer_index++] = seg_id;
		//     }
		//   }
		// }

		// Segment * volatile * repp = &q->HSeg;
		// Segment * rep = q->HSeg;
		// int skip;
		// for (int i = 0; i < (max_id - guard_id); i++) {
		//   skip = 0;
		//   if (rep->id >= max_id)
		//     break;
		//   for (int j = 0; j < id_buffer_len; j++) {
		//     if (id_buffer[j] == rep->id) {
		//       skip = 1;
		//       break;
		//     }
		//   }
		//   if (skip == 1) {
		//     repp = &rep->next;
		//     rep = rep->next;
		//   } else {
		//     *repp = rep->next;
		//     free(rep);
		//   }
		// }

Sleep:
		usleep(100);
	} // End of while(gc_goflag)

	pthread_exit("Garbage Collection thread is leaving.");
}
