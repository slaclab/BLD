#include <stdio.h>
#include <string.h>

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverImb.h"

IOSCANPVT bldImbIoscan;

int imb_create(BLDMCastReceiver **bld_receiver) {

  int status = bld_receiver_create(bld_receiver, sizeof(BLDImb) * 10,
				   BLD_IMB_PARAMS, BLD_IMB_GROUP,
				   BLD_IMB_PORT);

  if (status < 0) {
    printf("ERROR: Failed on bld_receiver_create (status=%d)\n", status);
    return status;
  }

  (*bld_receiver)->run = imb_run;
  (*bld_receiver)->report = imb_report;
  printf("INFO: Imb receiver at 0x%x (run 0x%x, report 0x%x)",
	 (int) bld_receiver, 0, 0); /*(*bld_receiver)->run,
	 (*bld_receiver)->report);
  */
  scanIoInit(&bldImbIoscan);

  return 0;
}

void imb_report(void *bld_receiver, int level) {
  if (bld_receiver != NULL & level > 2) {
    BLDMCastReceiver *receiver = bld_receiver;
    printf("*** Imb BLD ***\n");
    bld_receiver_report(bld_receiver, level);

    BLDImb *imb = receiver->bld_payload_bsa;
    BLDHeader *header = receiver->bld_header_bsa;
    
    if (imb != NULL && header != NULL) {
      printf("IMB PULSEID         : %d\n", __ld_le32(&(header->fiducialId)));
      printf("IMB SUM         : %f\n", __ld_le64(&(imb->sum)));
      printf("IMB XPOS         : %f\n", __ld_le64(&(imb->xpos)));
      printf("IMB YPOS        : %f\n", __ld_le64(&(imb->ypos)));
      printf("IMB CHANNEL10        : %f\n", __ld_le64(&(imb->channel10)));
      printf("IMB CHANNEL11        : %f\n", __ld_le64(&(imb->channel11)));	  
      printf("IMB CHANNEL12        : %f\n", __ld_le64(&(imb->channel12)));
      printf("IMB CHANNEL13        : %f\n", __ld_le64(&(imb->channel13)));	  	  
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
void imb_run(void *bld_receiver) {
  if (bld_receiver == NULL) {
    fprintf(stderr, "ERROR: Can't run Imb, got NULL parameter!\n");
    return;
  }

  BLDMCastReceiver *this = bld_receiver;
  while(1) {
    /** Get the next BLD from the message queue (saved to the bld_*_bsa */
    if (bld_receiver_next(bld_receiver) == 0) {    
      BLDImb *imb = this->bld_payload_bsa;
      BLDHeader *header = this->bld_header_bsa;
    
      epicsMutexLock(this->mutex);
      this->packets_processed++;
      
#ifdef SIGNAL_TEST
      /** TEST_CODE --- begin */
      __st_le64(&(imb->sum), (double)this->packets_received);
      __st_le64(&(imb->xpos), (double)this->packets_received + 1);
      __st_le64(&(imb->ypos), (double)this->packets_received + 2);
      __st_le64(&(imb->channel10), (double)this->packets_received + 3);
      __st_le64(&(imb->channel11), (double)this->packets_received + 4);	  
      __st_le64(&(imb->channel12), (double)this->packets_received + 5);
      __st_le64(&(imb->channel13), (double)this->packets_received + 6);	  	  
      /** TEST_CODE --- end */
#endif
      
      scanIoRequest(bldImbIoscan);
      epicsMutexUnlock(this->mutex);
    }
  }
}

