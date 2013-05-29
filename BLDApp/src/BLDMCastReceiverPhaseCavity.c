/* $Id: BLDMCastReceiverPhaseCavity.c,v 1.1.2.1 2013/05/24 22:12:06 lpiccoli Exp $ */

#include <stdio.h>
#include <string.h>

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

IOSCANPVT bldPhaseCavityIoscan;

int phase_cavity_create(BLDMCastReceiver **bld_receiver) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDPhaseCavity), BLD_PHASE_CAVITY_PARAMS);
  if (status < 0) {
    return status;
  }

  (*bld_receiver)->run = phase_cavity_run;
  (*bld_receiver)->report = phase_cavity_report;

  scanIoInit(&bldPhaseCavityIoscan);

  return 0;
}

void phase_cavity_report(void *bld_receiver, int level) {
  BLDMCastReceiver *receiver = bld_receiver;
  printf("*** PhaseCavity BLD ***\n");
  bld_receiver_report(bld_receiver, level);
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
  if (bld_receiver == NULL) {
    fprintf(stderr, "ERROR: Can't run PhaseCavity, got NULL parameter!\n");
    return;
  }

  BLDMCastReceiver *this = bld_receiver;
  while(1) {
    /** Get the next BLD from the message queue (saved to the bld_*_bsa */
    if (bld_receiver_next(bld_receiver) == 0) {    
      BLDPhaseCavity *pcav = this->bld_payload_bsa;
      BLDHeader *header = this->bld_header_bsa;
    
      epicsMutexLock(this->mutex);
      this->packets_processed++;
      
      /** TEST_CODE --- begin */
      __st_le64(&(pcav->charge1), (double)this->packets_received);
      __st_le64(&(pcav->charge2), (double)this->packets_received + 1);
      __st_le64(&(pcav->fitTime1), (double)this->packets_received + 2);
      __st_le64(&(pcav->fitTime2), (double)this->packets_received + 3);
      /** TEST_CODE --- end */
      
      scanIoRequest(bldPhaseCavityIoscan);
      epicsMutexUnlock(this->mutex);
    }
  }
}

