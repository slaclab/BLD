/* $Id: BLDMCastReceiver.h,v 1.1.2.4 2013/06/11 16:02:05 lpiccoli Exp $ */

#ifndef _BLDMCASTRECEIVER_H_
#define _BLDMCASTRECEIVER_H_

#include "BLDData.h"
#include "epicsMessageQueue.h"

/**
 * BLDMCastReceiver.h
 *
 * Definition of functions and data fields to handle incoming BLD data packets.
 *
 * For each type of BLD packets there is one instance of this struct.
 *
 * Two tasks are created to receive and process BLD packets. The first task
 * (function bld_receiver_run()) waits for multicast data and saves it
 * to a message queue. The second task keeps removing the BLD packets from
 * the queue and invoking ioScanRequest to update the BSA PVs.
 */
struct BLDMCastReceiver {
  /** Socket for receiving multicast BLD packets */
  int sock;

  /** Multicast address (string) */
  char *multicast_group;

  /** Port number */
  int port;

  /** Latest BLD header - passed to recvmsg */
  BLDHeader *bld_header_recv; 

  /** Latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_recv; 

  /**
   * Copy of the latest BLD header, used by the BSA device support
   * to fill in the PVs. This memory is shared between the device
   * support and the receiver.
   */
  BLDHeader *bld_header_bsa; 

  /* Copy of the latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_bsa; 

  /** Size of BLD data following the header */
  int payload_size; 

  /** Number of PVs to be updated (Number of items in BLD payload */
  int payload_count;

  /** Message queue contaiting BLD received packets */
  epicsMessageQueueId queue;

  /** Mutex for controlling access to this struct */
  epicsMutexId mutex;

  /** Count number of failed queue.trySend() */
  long queue_fail_send_count;

  /** Count number of failed queue.receive() */
  long queue_fail_receive_count;

  /** BSA PV counter - used to verify all BSA PVs were written */
  int bsa_counter;

  /** Count missing BLDs based on differences between received PULSEIDs */
  long missing_bld_counter;
  
  /** Number of BLD packets received */
  long packets_received;

  /** Number of BLD packets processed */
  long packets_processed;

  /** Current BSA PULSEID - used to make sure BSA buffers get the same data */
  unsigned int bsa_pulseid;

  /** Current BLD PULSEID - used to verify if there are missing BLD packets */
  unsigned int bld_pulseid;

  /** Counts number of BSA PULSEID mismatches */
  long bsa_pulseid_mismatch;

  /** Time when the last BLD packet was received */
  struct timeval bld_received_time;

  /** Max time elapsed (in usec) between BLD packets */
  epicsUInt32 bld_max_received_delay_us;

  /** Min time elapsed (in usec) between BLD packets */
  epicsUInt32 bld_min_received_delay_us;

  /** Average time elapsed (in usec) between BLD packets */
  epicsUInt32 bld_avg_received_delay_us;

  /** Count number of time elapsed 3x greater than the average */
  epicsUInt32 bld_received_delay_above_avg_counter;

  /** Print status information (dbior command) */
  void (*report)(void *bld_receiver, int level); 
  
  /** Run the BLD receiver */
  void (*run)(void *bld_receiver);
};

typedef struct BLDMCastReceiver BLDMCastReceiver;

int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count,
			char *multicast_group, int port);
int bld_receiver_destroy(BLDMCastReceiver *this);
int bld_receiver_next(BLDMCastReceiver *this);
void bld_receiver_report(void *this, int level);
void bld_receiver_run(BLDMCastReceiver *this);

void bld_receivers_start();
void bld_receivers_report(int level);

#endif
