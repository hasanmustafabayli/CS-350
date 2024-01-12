/*******************************************************************************
* Time Functions Library (implementation)
*
* Description:
*     A library to handle various time-related functions and operations.
*
* Author:
*     Renato Mancuso <rmancuso@bu.edu>
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2023
*
* Notes:
*     Ensure to link against the necessary dependencies when compiling and
*     using this library. Modifications or improvements are welcome. Please
*     refer to the accompanying documentation for detailed usage instructions.
*
*******************************************************************************/

#include "timelib.h"




uint64_t get_elapsed_sleep(long sec, long nsec){
  uint64_t star, end;
  get_clocks(star);
  struct timespec duration, duration2;
  duration.tv_sec = sec;
  duration.tv_nsec = nsec;
  nanosleep(&duration,&duration2);
  get_clocks(end);
  return end-star;
}

uint64_t get_elapsed_busywait(long sec, long nsec){
  uint64_t before, after;
  struct timespec begin,end;
  clock_gettime(CLOCK_MONOTONIC,&begin);
  get_clocks(before);
  end.tv_sec=sec;
  end.tv_nsec=nsec;
  timespec_add(&end, &begin);
  while (timespec_cmp(&end,&begin)>0) {
    clock_gettime(CLOCK_MONOTONIC,&begin);
  }
  get_clocks(after);
  uint64_t cycles=after-before;
  return cycles;
}

/* Utility function to add two timespec structures together. The input
 * parameter a is updated with the result of the sum. */
void timespec_add (struct timespec * a, struct timespec * b)
{
	/* Try to add up the nsec and see if we spill over into the
	 * seconds */
	time_t addl_seconds = b->tv_sec;
	a->tv_nsec += b->tv_nsec;
	if (a->tv_nsec > NANO_IN_SEC) {
		addl_seconds += a->tv_nsec / NANO_IN_SEC;
		a->tv_nsec = a->tv_nsec % NANO_IN_SEC;
	}
	a->tv_sec += addl_seconds;
}

/* Utility function to compare two timespec structures. It returns 1
 * if a is in the future compared to b; -1 if b is in the future
 * compared to a; 0 if they are identical. */
int timespec_cmp(struct timespec *a, struct timespec *b)
{
	if(a->tv_sec == b->tv_sec && a->tv_nsec == b->tv_nsec) {
		return 0;
	} else if((a->tv_sec > b->tv_sec) ||
		  (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec)) {
		return 1;
	} else {
		return -1;
	}
}

/* Busywait for the amount of time described via the delay
 * parameter */
/*uint64_t busywait_timespec(struct timespec delay)
{
  
}*/
