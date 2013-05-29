/* $Id: BLDMCastReceiver.c,v 1.1.2.1 2013/05/24 22:12:06 lpiccoli Exp $ */

#include <stdlib.h>
#include <stdio.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMessageQueue.h"

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

extern epicsEventId EVRFireEventPCAV;

/**
 * Allocate and initialize an instance of BLDMCastReceiver. The payload
 * space is reserved following the BLDMCastReceiver struct.
 *
 * @param bld_receiver output parameter with the pointer to the allocated struct
 * @param payload_size size in bytes of the BLD payload (e.g. size of 4 floats
 * for the phase cavity BLD)
 * @param payload_count number of parameters in the BLD payload (e.g. 4
 * parameters for the phase cavity BLD)
 *
 * @return 0 on success, -1 on failure
 */
int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count) {
  *this = (BLDMCastReceiver *) malloc(sizeof(BLDMCastReceiver));
  if (*this == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
    return -1;
  }

  (*this)->bld_header_recv = (BLDHeader *) malloc(sizeof(BLDHeader) + payload_size);
  if ((*this)->bld_header_recv == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
    return -1;
  }
  (*this)->bld_payload_recv = ((*this)->bld_header_recv) + 1;

  (*this)->bld_header_bsa = (BLDHeader *) malloc(sizeof(BLDHeader) + payload_size);
  if ((*this)->bld_header_bsa == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
    return -1;
  }
  (*this)->bld_payload_bsa = ((*this)->bld_header_bsa) + 1;

  (*this)->packets_received = 0;
  (*this)->packets_processed = 0;
  (*this)->payload_size = payload_size;
  (*this)->payload_count = payload_count;
  (*this)->bsa_counter = 0;
  (*this)->bsa_pulseid = 0;
  (*this)->bsa_pulseid_mismatch = 0;
  (*this)->queue_fail_send_count = 0;
  (*this)->queue_fail_receive_count = 0;

  (*this)->queue = epicsMessageQueueCreate(10, sizeof(BLDHeader) + (*this)->payload_size);
  if ((*this)->queue == NULL) {
    fprintf(stderr, "ERROR: Failed to create epicsMessageQueue\n");
    return -1;
  }

  (*this)->mutex = epicsMutexCreate();

  return 0;
}

int bld_receiver_destroy(BLDMCastReceiver *this) {
  if (this == NULL) {
    return -1;
  }

  epicsMessageQueueDestroy(this->queue);
  epicsMutexDestroy(this->mutex);

  free(this->bld_header_bsa);
  free(this->bld_header_recv);

  free(this);

  return -1;
}

extern EBEAMINFO bldEbeamInfo;

int bld_receiver_next(BLDMCastReceiver *this) {
  if (epicsMessageQueueReceive(this->queue, this->bld_header_bsa,
			       sizeof(BLDHeader) + this->payload_size) < 0) {
    epicsMutexLock(this->mutex);
    this->queue_fail_receive_count++;
    epicsMutexUnlock(this->mutex);
    return -1;
  }

  return 0;
}

void bld_receiver_report(void *this, int level) {
  BLDMCastReceiver *receiver = this;

  epicsMutexLock(receiver->mutex);

  printf("Received BLD packets : %ld\n", receiver->packets_received);
  printf("Processed BLD packets: %ld\n", receiver->packets_processed);
  printf("Pending BLD packets  : %d\n", epicsMessageQueuePending(receiver->queue));
  printf("Queue (failed send)  : %ld\n", receiver->queue_fail_send_count);
  printf("Queue (failed recv)  : %ld\n", receiver->queue_fail_receive_count);
  printf("BSA pulseId mismatch : %ld\n", receiver->bsa_pulseid_mismatch);

  if (level > 2) {
    epicsMessageQueueShow(receiver->queue, 4);
  }

  epicsMutexUnlock(receiver->mutex);
}

/**
 * This is the function invoked by the multicast receiver task.
 * It blocks waiting for BLD data. Once received the BLD is
 * copied to a shared message queue, which is later consumed
 * by another task (e.g. BLDPhaseCavity task).
 */
void bld_receiver_run(BLDMCastReceiver *this) {
  for (;;) {
    /** TEST_CODE --- begin */
    epicsEventWait(EVRFireEventPCAV); 
  
    epicsMutexLock(this->mutex);

    /** Saves timestamp in the received buffer - to be removed */
    BLDHeader *header = this->bld_header_recv;
    header->tv_sec = bldEbeamInfo.ts_sec;
    header->tv_nsec = bldEbeamInfo.ts_nsec;
    __st_le32(&(header->fiducialId), bldEbeamInfo.uFiducialId);
    __st_le32(&(header->damage), 0); 
    /** TEST_CODE --- end */

    if (epicsMessageQueueTrySend(this->queue, &header, sizeof(BLDHeader) + this->payload_size) != 0) {
      this->queue_fail_send_count++;
    }

    this->packets_received++;

    epicsMutexUnlock(this->mutex);
  }
}

BLDMCastReceiver *bldPhaseCavityReceiver;

void bld_receivers_report(int level) {
  phase_cavity_report(bldPhaseCavityReceiver, level);
}

/**
 * Create all BLD Multicast receivers and start the tasks.
 */
void bld_receivers_start() {
  phase_cavity_create(&bldPhaseCavityReceiver);
  epicsThreadMustCreate("BLDPhaseCavity", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bldPhaseCavityReceiver->run, bldPhaseCavityReceiver);
  epicsThreadMustCreate("BLDPhaseCavityProd", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bld_receiver_run, bldPhaseCavityReceiver);
}
