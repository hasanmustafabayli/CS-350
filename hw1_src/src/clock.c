/*******************************************************************************
* CPU Clock Measurement Using RDTSC
*
* Description:
*     This C file provides functions to compute and measure the CPU clock using
*     the `rdtsc` instruction. The `rdtsc` instruction returns the Time Stamp
*     Counter, which can be used to measure CPU clock cycles.
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 10, 2023
*
* Notes:
*      
*******************************************************************************/

/*******************************************************************************
* CPU Clock Measurement Using RDTSC
*
* Description:
*     This C file provides functions to compute and measure the CPU clock using
*     the `rdtsc` instruction. The `rdtsc` instruction returns the Time Stamp
*     Counter, which can be used to measure CPU clock cycles.
*
* Author:
*     [Your Name]
*
* Affiliation:
*     [Your Affiliation]
*
* Creation Date:
*     [Date]
*
* Notes:
*     Ensure that the platform supports the `rdtsc` instruction before using
*     these functions. Depending on the CPU architecture and power-saving
*     modes, the results might vary. Always refer to the CPU's official
*     documentation for accurate interpretations.
*
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "timelib.h"

/* uint64_t get_clocks(){ */
/*   uint32_t a,d; */
/* __asm__ __volatile__ ("rdtsc": "=a" (a), "=d" (d)); */
/*   return (((uint64_t)d << 32) | a); */
/*   } */

int main(int argc, char* argv[]) {
  (void)argc;
  long sec=atol(argv[1]);
  long nsec=atol(argv[2]);
  char method= *argv[3];

  if (method=='b') {
        
    uint64_t gul= get_elapsed_busywait(sec,nsec);
    printf("WaitMethod: %s\n", "BUSYWAIT");
    printf("WaitTime: %ld %ld \n", sec,nsec);
    printf("ClocksElapsed: %lu\n", gul);
    double speed = (double)sec + ((double)nsec)/1e9;

    printf("ClockSpeed: %.2f\n", (gul/(double)speed)*1e-6);

  } else if (method=='s'){
    uint64_t gul=get_elapsed_sleep(sec,nsec);
    printf("WaitMethod: %s\n","SLEEP");
    printf("WaitTime: %ld %ld\n",sec,nsec);
    printf("ClocksElapsed: %lu\n", gul);
    double speed=(double)sec + (double)nsec/1e9;
    printf("ClockSpeed: %.2f\n", (gul/(double)speed)*1e-6);
  } else {
    printf("try again!\n" );
  }
  return 0;
}







