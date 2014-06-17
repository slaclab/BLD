/* $Id: BLDMCastReceiverPhaseCavity.c,v 1.6 2014/06/05 19:51:33 scondam Exp $ */

#include <stdio.h>
#include <string.h>

#include <bsp/gt_timer.h>

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

IOSCANPVT bldPhaseCavityIoscan;

double bld_min_recv_delay_us = 0.;
double bld_max_recv_delay_us = 0.;

volatile unsigned bldPCAVReceiverMaxDelayUs = 0;
volatile unsigned bldPCAVReceiverMinDelayUs = 0;

int phase_cavity_create(BLDMCastReceiver **bld_receiver) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDPhaseCavity) * 10,
				   BLD_PHASE_CAVITY_PARAMS, BLD_PHASE_CAVITY_GROUP,
				   BLD_PHASE_CAVITY_PORT);

  if (status < 0) {
    printf("\nERROR: Failed on bld_receiver_create (status=%d)\n", status);
    return status;
  }

  (*bld_receiver)->run = phase_cavity_run;
  (*bld_receiver)->report = phase_cavity_report;
  printf("\nINFO: PhaseCavity receiver at 0x%x (run 0x%x, report 0x%x)\n",
	 (int) bld_receiver, 0, 0); /*(*bld_receiver)->run,
	 (*bld_receiver)->report);
  */
  scanIoInit(&bldPhaseCavityIoscan);

  return 0;
}

void phase_cavity_report(void *bld_receiver, int level) {
  if (bld_receiver != NULL & level > 2) {
    BLDMCastReceiver *receiver = bld_receiver;
    printf("*** PhaseCavity BLD ***\n");
    bld_receiver_report(bld_receiver, level);

    BLDPhaseCavity *pcav = receiver->bld_payload_bsa;
    BLDHeader *header = receiver->bld_header_bsa;
    
    if (pcav != NULL && header != NULL) {
      printf("PCAV PULSEID         : %d\n", __ld_le32(&(header->fiducialId)));
      printf("PCAV CHARGE1         : %f\n", __ld_le64(&(pcav->charge1)));
      printf("PCAV CHARGE2         : %f\n", __ld_le64(&(pcav->charge2)));
      printf("PCAV FITTIME1        : %f\n", __ld_le64(&(pcav->fitTime1)));
      printf("PAV FITTIME2        : %f\n", __ld_le64(&(pcav->fitTime2)));
    }
  }
}

extern EBEAMINFO bldEbeamInfo;

/**
 * Loop does not terminate. Processes after receiving a phase cavity
 * BLD package.
 *
 * Once received, the package is copied from the recv buffer into
 * the bsa buffer (accessed by the device support code).
 */
void phase_cavity_run(void *bld_receiver) {

epicsTimeStamp then, now;
double         remaining, diff;
double         diffus = 0.;

  if (bld_receiver == NULL) {
    fprintf(stderr, "ERROR: Can't run PhaseCavity, got NULL parameter!\n");
    return;
  }

  BLDMCastReceiver *this = bld_receiver;
  
  epicsTimeGetCurrent( &then );
    
  while(1) { 
    
    /** Get the next BLD from the message queue (saved to the bld_*_bsa */
    if (bld_receiver_next(bld_receiver) == 0) { 
	
	  epicsTimeGetCurrent( &now );		
	  	  
      BLDPhaseCavity *pcav = this->bld_payload_bsa;
      BLDHeader *header = this->bld_header_bsa;
    
      epicsMutexLock(this->mutex);
      this->packets_processed++;
      
      scanIoRequest(bldPhaseCavityIoscan);
      epicsMutexUnlock(this->mutex);	
	  
		diff = epicsTimeDiffInSeconds( &now, &then );		

		diffus = (double)(diff * 1000000.);
		
		if ( diffus > bld_max_recv_delay_us )
			bld_max_recv_delay_us = diffus;
			
		if ( diffus < bld_min_recv_delay_us )
			bld_min_recv_delay_us = diffus;			
  
		then = now;  	  
    }	
  	
  }
}

