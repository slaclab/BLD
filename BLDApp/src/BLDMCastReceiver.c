/* $Id: $ */

#include <stdlib.h>
#include <stdio.h>

#include "epicsThread.h"
#include "epicsEvent.h"

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
  (*this)->payload_size = payload_size;
  (*this)->payload_count = payload_count;
  (*this)->mutex = epicsMutexCreate();
  (*this)->mutex_counter = 0;

  return 0;
}

int bld_receiver_destroy(BLDMCastReceiver *this) {
  if (this == NULL) {
    return -1;
  }

  free(this->bld_header_bsa);
  free(this->bld_header_recv);
  
  free(this);

  return -1;
}

extern EBEAMINFO bldEbeamInfo;

int bld_receiver_next(BLDMCastReceiver *this) {
  int status = epicsEventWaitWithTimeout(EVRFireEventPCAV, 10); 
  if(status != epicsEventWaitOK) {
    return -1;
  }
  
  /** TEST_CODE --- begin */
  /** Saves timestamp in the received buffer - to be removed */
  BLDHeader *header = this->bld_header_recv;
  header->tv_sec = bldEbeamInfo.ts_sec;
  header->tv_nsec = bldEbeamInfo.ts_nsec;
  __st_le32(&(header->fiducialId), bldEbeamInfo.uFiducialId);
  __st_le32(&(header->damage), 0); 
  /** TEST_CODE --- end */

  this->packets_received++;
    
  return -1;
}

void bld_receiver_report(void *this, int level) {
  BLDMCastReceiver *receiver = this;
  printf("Received BLD packets: %d\n", receiver->packets_received);
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
  epicsThreadMustCreate("BLDPhaseCavity", epicsThreadPriorityMax, 20480,
			(EPICSTHREADFUNC)bldPhaseCavityReceiver->run, bldPhaseCavityReceiver);
}
