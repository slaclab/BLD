/* $Id: BLDMCastReceiverPhaseCavity.c,v 1.9 2015/04/01 16:39:03 scondam Exp $ */

#include <stdio.h>
#include <string.h>

#ifdef RTEMS
#include <bsp/gt_timer.h>
#endif

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

IOSCANPVT bldPhaseCavityIoscan;
IOSCANPVT bldPhaseCavityTestIoscan;

int phase_cavity_create(BLDMCastReceiver **bld_receiver, char *multicast_group) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDPhaseCavity) * 10,
				   BLD_PHASE_CAVITY_PARAMS,  multicast_group,
				   BLD_PHASE_CAVITY_PORT);

  if (status < 0) {
    printf("\nERROR: Failed on bld_receiver_create (status=%d)\n", status);
    return status;
  }

  (*bld_receiver)->report = phase_cavity_report;
  printf("\nINFO: PhaseCavity receiver at 0x%p\n", bld_receiver);

  if (strcmp(multicast_group,"239.255.24.1")== 0)
  	scanIoInit(&bldPhaseCavityIoscan);
  else if (strcmp(multicast_group,"239.255.24.254")== 0)
  	scanIoInit(&bldPhaseCavityTestIoscan);	

  return 0;
}

void phase_cavity_report(void *bld_receiver, int level)
{
  if (bld_receiver != NULL && level > 2) {
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



