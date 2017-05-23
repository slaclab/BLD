/* $Id: BLDMCastReceiverGdet.c,v 1.9 2015/04/01 16:39:03 scondam Exp $ */

#include <stdio.h>
#include <string.h>

#ifdef RTEMS
#include <bsp/gt_timer.h>
#endif

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverGdet.h"

IOSCANPVT bldFEEGasDetEnergyIoscan;

int gdet_create(BLDMCastReceiver **bld_receiver, char *multicast_group) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDGdet) * 10,
				   BLD_GDET_PARAMS,  multicast_group,
				   BLD_GDET_PORT);

  if (status < 0) {
    printf("\nERROR: Failed on bld_receiver_create (status=%d)\n", status);
    return status;
  }

  (*bld_receiver)->report = gdet_report;
  printf("\nINFO: Gdet receiver at 0x%x\n",
	 (int) bld_receiver);

  if (strcmp(multicast_group,"239.255.24.2")== 0)
  	scanIoInit(&bldFEEGasDetEnergyIoscan);

  return 0;
}

void gdet_report(void *bld_receiver, int level) {
  if ((bld_receiver != NULL) & (level > 2)) {
    BLDMCastReceiver *receiver = bld_receiver;
    printf("*** Gdet BLD ***\n");
    bld_receiver_report(bld_receiver, level);

    BLDGdet *gdet = receiver->bld_payload_bsa;
    BLDHeader *header = receiver->bld_header_bsa;
    
    if (gdet != NULL && header != NULL) {
      printf("GDET PULSEID         : %d\n", __ld_le32(&(header->fiducialId)));
      printf("GDET ENRC_11         : %f\n", __ld_le64(&(gdet->f_ENRC_11)));
      printf("GDET ENRC_12         : %f\n", __ld_le64(&(gdet->f_ENRC_12)));
      printf("GDET ENRC_21         : %f\n", __ld_le64(&(gdet->f_ENRC_21)));
      printf("GDET ENRC_22         : %f\n", __ld_le64(&(gdet->f_ENRC_22)));
      printf("GDET ENRC_63         : %f\n", __ld_le64(&(gdet->f_ENRC_63)));
      printf("GDET ENRC_64         : %f\n", __ld_le64(&(gdet->f_ENRC_64)));	  
    }
  }
}



