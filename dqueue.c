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

	/* This producer is the only thread who can update variables
	 * *local_head* and *local_tail* */
	while (th->local_head != th->local_tail) {

		/* Following READ_ONCE is to prevent compilers from performing too much
		 * optimizations.  E.g., moving the assignment statement out of while loop. */
		head = READ_ONCE(th->local_head);

		/* pos and value are retrieved in the reverse order that they were written into memory. */
		tmp_pos = th->local_buffer[head].enqueue_pos;
		tmp_value = th->local_buffer[head].value;

		seg = find_segment(th->HSeg, tmp_pos, th);
		th->HSeg = seg;
		DATA_TYPE * cell = &(seg->queue_data[tmp_pos % DQUEUE_SEG_SIZE]);
		*cell = tmp_value;

		/* local_head is only updated by this producer thread. So we can omit a memory fence here. */
		th->local_head = NEXT(th->local_head, LOCAL_BUFFER_SIZE);
	}
}

void help_enqueue(queue_t * q)
{
	handle_t * tmp_handle = producers;
	while (tmp_handle != NULL) {
		long tmp_head = tmp_handle->local_head;
		long tmp_tail = tmp_handle->local_tail;
		for (int i = 0; (i < LOCAL_BUFFER_SIZE) &&
				(WRAP(tmp_head + i, LOCAL_BUFFER_SIZE) != tmp_tail); i++) {
			/* Since an enqueue request is two 64-bit word (pos, value) that aren't
			 * read or written atomically, when an enqueue helper reads the two words,
			 * it must identify if both belong to the same logical request. A helper
			 * reads an enqueue request's two words in the reverse order they were 
			 * written, such that the data read belongs to request *tmp_pos* or a later
			 * request. If the data read is for a later request, a value has been 
			 * written into cell *tmp_pos*, and the program will notice this and return
			 * by checking if (*cell == BOT). */
			long head = WRAP(tmp_head+i, LOCAL_BUFFER_SIZE);


			/* ACQUIRE could be replaced by a smp_rmb right after this line.
		 	* We choose ACQUIRE for a fair comparison against other queue implementations. */
			long tmp_pos = ACQUIRE(&tmp_handle->local_buffer[head].enqueue_pos);

			/* pos and value are retrieved in the reverse order that they were written into memory. */
			DATA_TYPE tmp_value = tmp_handle->local_buffer[head].value;

			Segment * seg = find_segment(tmp_handle->HSeg, tmp_pos, tmp_handle);
			if (seg->id > tmp_pos / DQUEUE_SEG_SIZE) {
				/* Corresponding producer is making progress.   **
				 ** There is no need to help it.                */
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
	/* RELEASE could be replaced by a smp_wmb on this line.				*
	 * We choose RELEASE for a fair comparison against other queue implementations. *
	 * This RELEASE pairs with the ACQUIRE in help_enqueue().			*/
	RELEASE(&handle->local_buffer[tail].enqueue_pos, FAA(&q->tail, 1));

	/* local_tail is only updated by this producer thread. So we can omit a memory fence here. */
	handle->local_tail = NEXT(handle->local_tail, LOCAL_BUFFER_SIZE);
}

void * dequeue(queue_t * q, handle_t * th)
{
	Segment * seg = find_segment(th->HSeg, q->head, th);
	th->HSeg = seg;
	DATA_TYPE * cell = &(seg->queue_data[q->head % DQUEUE_SEG_SIZE]);

	/* Following READ_ONCE is to prevent compilers from performing too much
	 * optimizations.  E.g., moving the assignment statement out of while loop. */
	while (READ_ONCE(*cell) == BOT) {
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
	/* head is only updated by consumer thread. So we can omit a memory fence here. */
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
			free(tmp);
		}
	}
}

#include "gc.h"
