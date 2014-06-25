/* $Id: BLDMCastReceiverPhaseCavity.c,v 1.7 2014/06/17 23:32:50 scondam Exp $ */

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

int phase_cavity_create(BLDMCastReceiver **bld_receiver, char *multicast_group) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDPhaseCavity) * 10,
				   BLD_PHASE_CAVITY_PARAMS,  multicast_group,
				   BLD_PHASE_CAVITY_PORT);

  if (status < 0) {
    printf("\nERROR: Failed on bld_receiver_create (status=%d)\n", status);
    return status;
  }

  (*bld_receiver)->report = phase_cavity_report;
  printf("\nINFO: PhaseCavity receiver at 0x%x\n",
	 (int) bld_receiver);

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



