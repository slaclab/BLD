/* $Id: BLDMCastReceiverPhaseCavity.c,v 1.1.2.6 2013/06/11 16:02:05 lpiccoli Exp $ */

#include <stdio.h>
#include <string.h>

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

IOSCANPVT bldPhaseCavityIoscan;

int phase_cavity_create(BLDMCastReceiver **bld_receiver) {
  int status = bld_receiver_create(bld_receiver, sizeof(BLDPhaseCavity) * 10,
				   BLD_PHASE_CAVITY_PARAMS, BLD_PHASE_CAVITY_GROUP,
				   BLD_PHASE_CAVITY_PORT);
  if (status < 0) {
    printf("ERROR: Failed on bld_receiver_create (status=%d)\n", status);
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

  BLDPhaseCavity *pcav = receiver->bld_payload_bsa;
  BLDHeader *header = receiver->bld_header_bsa;

  printf("PCAV PULSEID         : %d\n", __ld_le32(&(header->fiducialId)));
  printf("PCAV CHARGE1         : %f\n", __ld_le64(&(pcav->charge1)));
  printf("PCAV CHARGE2         : %f\n", __ld_le64(&(pcav->charge2)));
  printf("PCAV FITTIME1        : %f\n", __ld_le64(&(pcav->fitTime1)));
  printf("PCAV FITTIME2        : %f\n", __ld_le64(&(pcav->fitTime2)));
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
      
#ifdef SIGNAL_TEST
      /** TEST_CODE --- begin */
      __st_le64(&(pcav->charge1), (double)this->packets_received);
      __st_le64(&(pcav->charge2), (double)this->packets_received + 1);
      __st_le64(&(pcav->fitTime1), (double)this->packets_received + 2);
      __st_le64(&(pcav->fitTime2), (double)this->packets_received + 3);
      /** TEST_CODE --- end */
#endif
      
      scanIoRequest(bldPhaseCavityIoscan);
      epicsMutexUnlock(this->mutex);
    }
  }
}

