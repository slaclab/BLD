/* $Id: $ */

#ifndef _BLDMCASTRECEIVER_H_
#define _BLDMCASTRECEIVER_H_

#include "BLDData.h"

/**
 * BLDMCastReceiver.h
 *
 * Definition of functions to handle incoming BLD data packets
 */
struct BLDMCastReceiver {
  /** Latest BLD header - passed to recvmsg */
  BLDHeader *bld_header_recv; 

  /** Latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_recv; 

  /**
   * Copy of the latest BLD header, used by the BSA device support
   * to fill in the PVs. This memory is shared between the device
   * support and the receiver. The access is controlled by the
   * mutex (declared below).
   */
  BLDHeader *bld_header_bsa; 

  /* Copy of the latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_bsa; 

  /** Size of BLD data following the header */
  int payload_size; 

  /** Number of PVs to be updated (Number of items in BLD payload */
  int payload_count;

  /** Controls access to BLD data (Receiver vs Records) */
  epicsMutexId mutex; 

  /** Mutex counter - used to release mutex */
  int mutex_counter;

  /** Number of BLD packets received */
  long packets_received;

  /** Print status information (dbior command) */
  void (*report)(void *bld_receiver, int level); 
  
  /** Run the BLD receiver */
  void (*run)(void *bld_receiver);
};

typedef struct BLDMCastReceiver BLDMCastReceiver;

int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count);
int bld_receiver_destroy(BLDMCastReceiver *this);
int bld_receiver_next(BLDMCastReceiver *this);
void bld_receiver_report(void *this, int level);

void bld_receivers_start();
void bld_receivers_report(int level);

#endif
