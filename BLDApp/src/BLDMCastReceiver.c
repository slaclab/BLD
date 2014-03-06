/* $Id: BLDMCastReceiver.c,v 1.3 2014/03/03 21:15:14 lpiccoli Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>

#include "epicsThread.h"
#include "epicsEvent.h"
#include "epicsMessageQueue.h"
#include "initHooks.h"

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

#define BLD_FB05_ETH0 "172.27.2.185"
#define BLD_IOC_ETH0 "172.27.2.162"

#ifdef SIGNAL_TEST
extern epicsEventId EVRFireEventPCAV;
#endif

static void check(char *nm, void *ptr) {
  if (devBusMappedRegister(nm, ptr) == 0) {
    errlogPrintf("WARNING: Unable to register '%s'; related statistics may not work\n", nm);
  }
}

static void bld_hook_function(initHookState state) {
  switch (state) {
  case initHookAtBeginning:
    bld_receivers_start();
    break;
  default:
    break;
  }
}

int bld_hook_init() {
  return (initHookRegister(bld_hook_function));
}

static void address_to_string(unsigned int address, char *str) {
  unsigned int networkAddr = htonl(address);
  const unsigned char* pcAddr = (const unsigned char*) &networkAddr;

  sprintf(str, "%u.%u.%u.%u", pcAddr[0], pcAddr[1], pcAddr[2], pcAddr[3]);
}

static int create_socket(unsigned int address, unsigned int port, int receive_buffer_size) {
  int sock = -1;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    printf("ERROR: Failed to create socket (errno=%d)\n", errno);
    return -1;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		 &receive_buffer_size, sizeof(receive_buffer_size)) == -1) {
    printf("ERROR: Failed to set socket receiver buffer size (errno=%d)\n", errno);
    return -1;
  }

  int reuse = 1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		 &reuse, sizeof(reuse)) == -1) {
    printf("ERROR: Failed to set socket reuse address option (errno=%d)\n", errno);
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec = 60;
  timeout.tv_usec = 0;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
		 &timeout, sizeof(timeout)) == -1) {
    printf("ERROR: Failed to set socket timeout option (errno=%d)\n", errno);
    return -1;
  }

  struct sockaddr_in sockaddrSrc;
  sockaddrSrc.sin_family = AF_INET;
  sockaddrSrc.sin_addr.s_addr = htonl(address);
  sockaddrSrc.sin_port = htons(port);
  
  if (bind(sock, (struct sockaddr*) &sockaddrSrc, sizeof(sockaddrSrc)) == -1) {
    printf("ERROR: Failed to bind socket (errno=%d)\n", errno);
  }

  struct sockaddr_in sockaddrName;
  socklen_t len = sizeof(sockaddrName);
  if(getsockname(sock, (struct sockaddr *) &sockaddrName, &len) == 0) {
    unsigned int sockAddress = ntohl(sockaddrName.sin_addr.s_addr);
    unsigned int sockPort = (unsigned int )ntohs(sockaddrName.sin_port);
    char str[100];
    address_to_string(sockAddress, str);
    printf( "Server addr: %s  Port %u  Buffer Size %u\n",
	    str, sockPort, receive_buffer_size);
  }
  else {
    printf("ERROR: Failed on getsockname() (errno=%d)\n", errno);
    return -1;
  }

  return sock;
}

/**
 * @param address multicast group address
 */
static int register_multicast(int sock, unsigned int address) {
  unsigned int interface;

#ifdef FB05_TEST
  char *interface_string = BLD_FB05_ETH0;
#else
#error Are you really compiling for production? If so remove me!
  char *interface_string = BLD_IOC_ETH0;
#endif

  struct in_addr inp;
  if (inet_aton(interface_string, &inp) == 0) {
    printf("ERROR: Failed on inet_aton() (errno=%d)\n", errno);
    return -1;
  }

  interface = ntohl(inp.s_addr);

  if (interface != 0) {
    char str[100];
    address_to_string(interface, str);
    printf("Multicast interface IP: %s (interface %s) %u\n",
	   str, interface_string, interface);

    struct ip_mreq ipMreq;
    memset((char*)&ipMreq, 0, sizeof(ipMreq));
    ipMreq.imr_multiaddr.s_addr = htonl(address);
    ipMreq.imr_interface.s_addr = htonl(interface);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&ipMreq,
		   sizeof(ipMreq)) < 0 ) {
      printf("ERROR: Failed to set socket multicast option (errno=%d)\n", errno);
      return -1;
    }
  }
  else {
    printf("ERROR: Failed on ntohl(inet_addr(...)) (errno=%d)\n", errno);
    return -1;
  }
  
  return 0;
}

static int bld_register_mulitcast(BLDMCastReceiver *this) {
  int inet_address = inet_addr(this->multicast_group);
  if (inet_address == -1) {
    printf("ERROR: Failed on inet_addr(\"%s\")\n", this->multicast_group);
    return -1;
  }

  this->sock = create_socket(ntohl(inet_address), this->port,
			     sizeof(BLDHeader) + this->payload_size);
  if (this->sock < 0) {
    printf("ERROR: Failed to create socket for group \"%s\"\n", this->multicast_group);
    return -1;
  }

  printf("INFO: Registering to get BLD for group %s\n", this->multicast_group);

  if (register_multicast(this->sock, ntohl(inet_address)) < 0) {
    return -1;
  }


  return 0;
}

/**
 * Allocate and initialize an instance of BLDMCastReceiver. The payload
 * space is reserved following the BLDMCastReceiver struct.
 *
 * @param bld_receiver output parameter with the pointer to the allocated struct
 * @param payload_size size in bytes of the BLD payload (e.g. size of 4 floats
 * for the phase cavity BLD)
 * @param payload_count number of parameters in the BLD payload (e.g. 4
 * parameters for the phase cavity BLD)
 * @param multicast_group BLD multicast group address in dotted string form
 * @param port socket port
 *
 * @return 0 on success, -1 on failure
 */
int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count,
			char *multicast_group, int port) {
  *this = (BLDMCastReceiver *) malloc(sizeof(BLDMCastReceiver));
  if (*this == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
    return -1;
  }

  (*this)->bld_header_recv = (BLDHeader *) malloc(sizeof(BLDHeader) + payload_size );
  if ((*this)->bld_header_recv == NULL) {
    fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
    return -1;
  }
  (*this)->bld_payload_recv = ((*this)->bld_header_recv) + 1;

  (*this)->bld_header_bsa = (BLDHeader *) malloc(sizeof(BLDHeader) + payload_size );
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
  (*this)->missing_bld_counter = 0;
  (*this)->bld_pulseid = 0x1FFFF;
  (*this)->bsa_pulseid = 0x1FFFF;
  (*this)->bsa_pulseid_mismatch = 0;
  (*this)->queue_fail_send_count = 0;
  (*this)->queue_fail_receive_count = 0;
  (*this)->multicast_group = multicast_group;
  (*this)->port = port;
  (*this)->bld_max_received_delay_us = 0;
  (*this)->bld_min_received_delay_us = 0xFFFFFFF;
  (*this)->bld_avg_received_delay_us = 0;
  (*this)->bld_received_delay_above_avg_counter = 0;

  check("pcav_latmax_f", &(*this)->bld_max_received_delay_us);
  check("pcav_latmin_f", &(*this)->bld_min_received_delay_us);
  check("pcav_latavg_f", &(*this)->bld_avg_received_delay_us);
  check("pcav_latavgcnt_f", &(*this)->bld_received_delay_above_avg_counter);


#ifndef SIGNAL_TEST
  /** Create socket and register to multicast group */
  if (bld_register_mulitcast(*this) < 0) {
    free((*this)->bld_header_bsa);
    free((*this)->bld_header_recv);
    
    free(*this);
    *this = NULL;
    return -1;
  }
#endif

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
  if (receiver != NULL) {
    epicsMutexLock(receiver->mutex);

    printf("Received BLD packets : %ld\n", receiver->packets_received);
    printf("Processed BLD packets: %ld\n", receiver->packets_processed);
    printf("Pending BLD packets  : %d\n", epicsMessageQueuePending(receiver->queue));
    printf("Queue (failed send)  : %ld\n", receiver->queue_fail_send_count);
    printf("Queue (failed recv)  : %ld\n", receiver->queue_fail_receive_count);
    printf("BSA pulseId mismatch : %ld (indicates that BSA buffers got data from different pulseIds)\n",
	   receiver->bsa_pulseid_mismatch);
    printf("Avg delay between BLD: %ld usec\n", receiver->bld_avg_received_delay_us);
    printf("Max delay between BLD: %ld usec\n", receiver->bld_max_received_delay_us);
    printf("Min delay between BLD: %ld usec\n", receiver->bld_min_received_delay_us);
    printf("Delay 1.5x above avg : %ld packets\n", receiver->bld_received_delay_above_avg_counter);
    
    if (level > 2) {
      epicsMessageQueueShow(receiver->queue, 4);
    }
    
    epicsMutexUnlock(receiver->mutex);
  }
}

static epicsUInt32 us_since_last_bld(BLDMCastReceiver *this) {
  struct timeval now;

  gettimeofday(&now, 0);
	
  if (now.tv_usec < this->bld_received_time.tv_usec) {
    now.tv_usec += 1000000;
    now.tv_sec--;	
  }
  now.tv_usec  = now.tv_usec - this->bld_received_time.tv_usec;
  now.tv_usec += now.tv_sec  - this->bld_received_time.tv_sec;

  return (epicsUInt32) now.tv_usec;
}

static int bld_get_message(BLDMCastReceiver *this) {
  size_t recvSize = 0;
  struct msghdr msghdr;
  int flags = 0;

  struct iovec       iov; // Buffer description socket receive
  struct sockaddr_in src; // Socket name source machine

  iov.iov_base = this->bld_header_recv;
  iov.iov_len  = sizeof(BLDHeader) + this->payload_size ;

  memset((void*)&msghdr, 0, sizeof(msghdr));
  msghdr.msg_name = (caddr_t)&src;
  msghdr.msg_namelen = sizeof(src);
  msghdr.msg_iov = &iov;
  msghdr.msg_iovlen = 1;

/*   printf("INFO: Waiting for message of size %d, sock %d\n", iov.iov_len, this->sock); */
  recvSize = recvmsg(this->sock, &msghdr, flags);

  epicsUInt32 this_time = us_since_last_bld(this);

  if (this->bld_max_received_delay_us > 0) {
    if (this_time > this->bld_max_received_delay_us) {
      this->bld_max_received_delay_us = this_time;
    }
    if (this_time < this->bld_min_received_delay_us) {
      this->bld_min_received_delay_us = this_time;
    }

    /* Running average: fn+1 = 127/128 * fn + 1/128 * x = fn - 1/128 fn + 1/128 x */
    this->bld_avg_received_delay_us += ((int)(- this->bld_avg_received_delay_us + this_time)) >> 8;

    /** Skip first packets to count above 1.5x the average */
    if (this->packets_received > 100 &&
	this_time > this->bld_avg_received_delay_us * 1.5) {
      this->bld_received_delay_above_avg_counter++;
    }
  }
  else {
    this->bld_max_received_delay_us = 1; /* this is to skip the first measurement */
  }

  gettimeofday(&this->bld_received_time, 0);

  if (recvSize < 0) {
    printf("ERROR: Failed on recvmsg(...)) (errno=%d)\n", errno);
  }

  if (recvSize == 0) {
    printf("Message size: ZERO (errno=%d)\n", errno);
  }
  else {
    if (recvSize == -1) {
      if (errno == EAGAIN) {
	printf("No messages received for group %s, timed out. (errno=%d)\n",
	       this->multicast_group, errno);
      }
      else {
	printf("ERROR: No messages received (errno=%d)\n", errno);
      }
    }
    else {
/*       printf("Message size: %d\n", recvSize); */
    }
  }

  return recvSize;
}

/**
 * This is the function invoked by the multicast receiver task.
 * It blocks waiting for BLD data. Once received the BLD is
 * copied to a shared message queue, which is later consumed
 * by another task (e.g. BLDPhaseCavity task).
 */
void bld_receiver_run(BLDMCastReceiver *this) {
  epicsThreadSleep(20);
  printf("INFO: Waiting for BLD packets from group %s\n", this->multicast_group);
  for (;;) {
#ifdef SIGNAL_TEST
    /** TEST_CODE --- begin */
    epicsEventWait(EVRFireEventPCAV); /** REPLACE BY recvmsg() */
    /** TEST_CODE --- end */
#else
    /** Wait for BLD Multicast */
    if (bld_get_message(this) > 0) {
#endif
  
    epicsMutexLock(this->mutex);

    BLDHeader *header = this->bld_header_recv;

#ifdef SIGNAL_TEST
    /** TEST_CODE --- begin */
    /** Saves timestamp in the received buffer - to be removed */
    header->tv_sec = bldEbeamInfo.ts_sec;
    header->tv_nsec = bldEbeamInfo.ts_nsec;
    __st_le32(&(header->fiducialId), bldEbeamInfo.uFiducialId);
    __st_le32(&(header->damage), 0); 
    /** TEST_CODE --- end */
#endif

    if (epicsMessageQueueTrySend(this->queue, header,
				 sizeof(BLDHeader) + this->payload_size) != 0) {
      this->queue_fail_send_count++;
    }

    this->packets_received++;

    epicsMutexUnlock(this->mutex);
#ifndef SIGNAL_TEST
    }
#endif
  }
}

BLDMCastReceiver *bldPhaseCavityReceiver = NULL;

void bld_receivers_report(int level) {
  if (bldPhaseCavityReceiver != NULL) {
    phase_cavity_report(bldPhaseCavityReceiver, level);
  }
}

/**
 * Create all BLD Multicast receivers and start the tasks.
 */
void bld_receivers_start() {
  printf("INFO: Creating PhaseCavity Receiver... ");
  phase_cavity_create(&bldPhaseCavityReceiver);

  printf("INFO: Starting PhaseCavity Receiver... ");
  epicsThreadMustCreate("BLDPhaseCavity", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bldPhaseCavityReceiver->run, bldPhaseCavityReceiver);
  epicsThreadMustCreate("BLDPhaseCavityProd", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bld_receiver_run, bldPhaseCavityReceiver);
  printf(" done.\n ");
}
