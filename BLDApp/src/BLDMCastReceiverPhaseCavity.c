/* $Id: $ */

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
  epicsMutexLock(receiver->mutex);
  printf("*** PhaseCavity BLD ***\n");
  bld_receiver_report(bld_receiver, level);
  epicsMutexUnlock(receiver->mutex);
}

extern EBEAMINFO bldEbeamInfo;

/**
 * Loop does not terminate. Processes after receiving a phase cavity
 * BLD package.
 *
 * Once received, the package is copied from the recv buffer into
 * the bsa buffer (accessed by the device support code).
 *
 * Copy to the bsa buffer is controlled by the bld_receiver->mutex
 */
void phase_cavity_run(void *bld_receiver) {
  if (bld_receiver == NULL) {
    fprintf(stderr, "ERROR: Can't run PhaseCavity, got NULL parameter!\n");
    return;
  }

  BLDMCastReceiver *this = bld_receiver;
  while(1) {
    bld_receiver_next(bld_receiver);
    
    BLDPhaseCavity *pcav = this->bld_payload_recv;
    BLDHeader *header = this->bld_header_recv;

    /** TEST_CODE --- begin */
    __st_le64(&(pcav->charge1), (double)this->packets_received);
    __st_le64(&(pcav->charge2), (double)this->packets_received + 1);
    __st_le64(&(pcav->fitTime1), (double)this->packets_received + 2);
    __st_le64(&(pcav->fitTime2), (double)this->packets_received + 3);
    /** TEST_CODE --- end */

    epicsMutexLock(this->mutex);
    memcpy(this->bld_header_bsa, this->bld_header_recv, sizeof(BLDHeader) + sizeof(BLDPhaseCavity));
    scanIoRequest(bldPhaseCavityIoscan);
    epicsMutexUnlock(this->mutex);
  }
}

