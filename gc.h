
/* Sample code for garbage collection. */
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
			free(tobe_freed);
		}

		/* Garbage Collection for sublist [gard_id, max_id) */
		// The following code must be turned on if we want a wait-free memory management scheme.
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
