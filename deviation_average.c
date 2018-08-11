/*
 * clock deviation utility
 * Copyright (C) 2018  Ricardo Biehl Pasquali
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 21/03/2018
 *
 * Routines that calculate whether a synchronization must
 * be done between two isochronous events.
 *
 * We keep a history of the deviations of the secondary
 * event from the reference event. If more than a half of
 * the history is out-of-sync (deviations out of expected
 * range) we calculate the deviation average and return
 * it to the caller.
 *
 * e.g. of the possible situations
 *
 * Getting ahead:
 *
 * secondary: |       |       +|      +|      ++|     ++|
 * reference: |       |       |       |       |       |
 *
 * Getting behind:
 *
 * secondary: |       |      |-      |-     |--     |--
 * reference: |       |       |       |       |       |
 *
 * Synced (with some expected deviations):
 *
 * secondary: |       |      |-       |       +|      |
 * reference: |       |       |       |       |       |
 */

#include <stdlib.h> /* labs() function */

#include "deviation_history.h"
#include "more_than_half.h"

/*
 * Called on every reference event.
 *
 * If the deviation is out-of-sync (i.e. the absolute value
 * of the deviation is greater than max_deviation) it's
 * placed in the history and misses is incremented.
 *
 * If more than a half of the history is out-of-sync we
 * return the average deviation of all out-of-sync elements
 * to the caller which can apply the correction.
 *
 * NOTE: if the secondary event is generated by another host,
 *   it may take some time until the host sends the event
 *   synchronized because of the network delays.
 */
long
more_than_half_do_sync(struct sync *s, long deviation)
{
	long tail_deviation;

	/* TODO: make sure it does support a history of size 1 */
	tail_deviation = deviation_history_get_last(&s->deviation_history);

	s->total_sum -= tail_deviation;
	s->total_sum += deviation;

	/*
	 * keep the misses count and the deviation
	 * average of the out-of-sync elements
	 *
	 * labs(): absolute value for long type
	 */
	if (labs(tail_deviation) > s->max_deviation) {
		s->misses--;
		s->out_of_sync_sum -= tail_deviation;
	}
	if (labs(deviation) > s->max_deviation) {
		s->misses++;
		s->out_of_sync_sum += deviation;
	}

	deviation_history_insert(&s->deviation_history, deviation);

	/*
	 * if more than a half of history is out-of-sync
	 * return the average deviation
	 */
	if (s->misses > s->half_history_size)
		return s->out_of_sync_sum / s->misses;
	/* NOTE: we're returning int64_t instead of long */

	return 0;
}

int
more_than_half_reset(struct sync *s, void *history_addr,
                     unsigned int history_size,
                     unsigned long allowed_deviation)
{
	deviation_history_reset(&s->deviation_history, history_addr,
	                        history_size);

	s->half_history_size = history_size / 2;
	s->max_deviation = allowed_deviation;

	/* reset variables that keep track of the time deviations */
	s->misses = 0;
	s->out_of_sync_sum = 0;
	s->total_sum = 0;

	return 0;
}