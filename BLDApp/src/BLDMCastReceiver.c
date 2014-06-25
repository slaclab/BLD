/* $Id: BLDMCastReceiver.c,v 1.12 2014/06/17 23:32:50 scondam Exp $ */

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

#include <bsp/gt_timer.h>

#include <epicsThread.h>
#include <epicsEvent.h>
#include <initHooks.h>

#include <errlog.h>

#include <evrTime.h>
#include <evrPattern.h>
/*#include <fidProcess.h> */

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"
#include "BLDMCastReceiverImb.h"

#define BLD_BD02_ETH0 "172.27.2.203"
#define BLD_IOC_ETH0 "172.27.2.162"
#define BLD_B34_IOC_ETH0 "134.79.219.145"
#define BLD_B34_IOC1_ETH0 "134.79.218.187"

extern IOSCANPVT bldPhaseCavityIoscan;
extern IOSCANPVT bldHxxUm6Imb01Ioscan;
extern IOSCANPVT bldHxxUm6Imb02Ioscan;

typedef  int16_t  __attribute__ ((may_alias))  int16_t_a;
typedef uint32_t  __attribute__ ((may_alias)) uint32_t_a;

volatile unsigned bldFidProcessActiveTimeslots = TIMESLOT1_MASK | TIMESLOT4_MASK;

Uint32_LE Bld_Pulseid[2800] = {0};
epicsTimeStamp Bld_Pulseid_Timestamp[2800];
int		Bld_Pulseid_Count = 0;

epicsTimeStamp getTimeStamp(Uint32_LE pulseid)
{
	epicsTimeStamp epicsTs;
	
	epicsTs.secPastEpoch = 0;
	epicsTs.nsec = 0;
	
	int i;
	
	int curr = Bld_Pulseid_Count;
	
	for (i=0; i < 2800; i++) {
		if (Bld_Pulseid[abs(curr-i)] == pulseid) { epicsTs = Bld_Pulseid_Timestamp[i]; break; }
	}
	
	return epicsTs;
}

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

  int name_len = 100;
  char name[name_len];

  char *interface_string;
  
  int rtncode = gethostname(name, name_len);
  if (rtncode == 0) {
    if (strcmp("ioc-sys0-bd01", name) == 0) {
      interface_string = BLD_IOC_ETH0;
    }
    else {
      if (strcmp("ioc-b34-bd01", name) == 0) {
	interface_string = BLD_B34_IOC_ETH0;
      }
      else {
		if (strcmp("ioc-sys0-bd02", name) == 0) {
	  		interface_string = BLD_BD02_ETH0;
		}
		else {
			if (strcmp("ioc-b34-bd02", name) == 0) {
	  			interface_string = BLD_B34_IOC1_ETH0;
			}	
			else {
	  			printf("ERROR: BLD code running on unknown IOC\n");
	  			return -1;
			}
		}
      }
    }
  }
  else {
    printf("ERROR: Unable to get hostname (errno=%d, rtncode=%d)\n", errno, rtncode);
    return -1;
  }

  printf("INFO: ETH0 address is %s\n", interface_string);

  struct in_addr inp;
  if (inet_aton(interface_string, &inp) == 0) {
    printf("ERROR: Failed on inet_aton() (errno=%d)\n", errno);
    return -1;
  }

  interface = ntohl(inp.s_addr);

  if (interface != 0) {
    char str[100];
    address_to_string(interface, str);
    printf("Multicast interface IP: %s (interface %s) %u (RECEIVER)\n",
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
 * for the phase cavity BLD or size of 7 floats for Imb)
 * @param payload_count number of parameters in the BLD payload (e.g. 4
 * parameters for the phase cavity BLD or 7 parameters for Imb)
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
	(*this)->multicast_group = multicast_group;
	(*this)->port = port;
	(*this)->late_bld_pulse = 0;
	(*this)->miss_bld_pulse = 0;
	(*this)->last_bld_pulse = 0;
    (*this)->bld_diffus = 8333.;
    (*this)->bld_diffus_max = 8333.;
    (*this)->bld_diffus_min = 8333.;
    (*this)->bld_diffus_avg = 8333.;  
		
	if (strcmp(multicast_group,"239.255.24.1")== 0) {
		check("pcav_max_f", &(*this)->bld_diffus_max);
		check("pcav_min_f", &(*this)->bld_diffus_min);
		check("pcav_avg_f", &(*this)->bld_diffus_avg);
		check("pcav_late_f", &(*this)->late_bld_pulse);
		check("pcav_miss_f", &(*this)->miss_bld_pulse);						
	}
	else if (strcmp(multicast_group,"239.255.24.4")== 0) {
		check("HxxUm6Imb01_max_f", &(*this)->bld_diffus_max);
		check("HxxUm6Imb01_min_f", &(*this)->bld_diffus_min);
		check("HxxUm6Imb01_avg_f", &(*this)->bld_diffus_avg);	
		check("HxxUm6Imb01_late_f", &(*this)->late_bld_pulse);
		check("HxxUm6Imb01_miss_f", &(*this)->miss_bld_pulse);						
	}
	else if (strcmp(multicast_group,"239.255.24.5")== 0) {
		check("HxxUm6Imb02_max_f", &(*this)->bld_diffus_max);
		check("HxxUm6Imb02_min_f", &(*this)->bld_diffus_min);
		check("HxxUm6Imb02_avg_f", &(*this)->bld_diffus_avg);	
		check("HxxUm6Imb02_late_f", &(*this)->late_bld_pulse);
		check("HxxUm6Imb02_miss_f", &(*this)->miss_bld_pulse);							
	}	

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
	
	(*this)->mutex = epicsMutexCreate();  	
	
	return 0;
}

int bld_receiver_destroy(BLDMCastReceiver *this) {
  if (this == NULL) {
    return -1;
  }

  epicsMutexDestroy(this->mutex);

  free(this->bld_header_bsa);
  free(this->bld_header_recv);

  free(this);

  return -1;
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

  if (recvSize < 0) {
    errlogPrintf("ERROR: Failed on recvmsg(...)) (errno=%d)\n", errno);
  }

  if (recvSize == 0) {
    errlogPrintf("Message size: ZERO (errno=%d)\n", errno);
  }
  else {
    if (recvSize == -1) {
      if (errno == EAGAIN) {
		errlogPrintf("No messages received for group %s, timed out. (errno=%d)\n",
	       this->multicast_group, errno);
      }
      else {
		errlogPrintf("ERROR: No messages received (errno=%d)\n", errno);
      }
    }
  }

  return recvSize;
}

 /* ==========================================================================

    Auth: Shantha Condamoor
    Date: 24-Jun-2014

    Name: bld_receivers_start()

    Comments: This is the function invoked by the multicast receiver task.
			It blocks waiting for BLD data. 
			Once BLD is received, it is double buffered and passed directly device support.
			Timestamp for device support is obtained from Fiducial Process routine.
			This timestmap replaces the BLD header timestamp if there is pulseId match.
			Pulse Id mis-matches are not handled yet correctly.

============================================================================= */

void bld_receiver_run(BLDMCastReceiver *this) {

    int         rc;
    epicsTimeStamp epicsTs;
    epicsTimeStamp epicsTsPulseId;   
	
	BLDHeader *header = NULL;  
  
    printf("BLD_RECEIVER_RUN()\n");
  
    printf("INFO: Waiting for BLD packets from group %s\n", this->multicast_group);	
	
  	epicsThreadSleep(5);	
	
    int curr = Bld_Pulseid_Count;		 	

    this->previous_bld_time = Bld_Pulseid_Timestamp[curr];	
	
	for (;;) {
	
    	/** Wait for BLD Multicast */
    	if (bld_get_message(this) > 0) {

            /* BLD packet received time */
        	rc = evrTimeGetFromPipeline(&epicsTs, evrTimeActive, 0, 0, 0, 0, 0);		

        	epicsMutexLock(this->mutex);
			
            	this->packets_received++;
				
				this->bld_recv_time = epicsTs;			/* current BLD packet received time */				
			
    			header = this->bld_header_recv;	
				this->bld_header_recv = this->bld_header_bsa;		
				this->bld_header_bsa = header;

  	  			if (strcmp(this->multicast_group,"239.255.24.1")== 0)  {
				   BLDPhaseCavity *payload = (BLDPhaseCavity *) this->bld_payload_recv;
				   this->bld_payload_recv = this->bld_payload_bsa;
				   this->bld_payload_bsa = payload;				   
				}
  	  			else if (strcmp(this->multicast_group,"239.255.24.4")== 0)   {  
				   BLDImb *payload = (BLDImb *) this->bld_payload_recv;
				   this->bld_payload_recv = this->bld_payload_bsa;
				   this->bld_payload_bsa = payload;					   
				}
  	  			else if (strcmp(this->multicast_group,"239.255.24.5")== 0) { 
				   BLDImb *payload = (BLDImb *) this->bld_payload_recv;
				   this->bld_payload_recv = this->bld_payload_bsa;
				   this->bld_payload_bsa = payload;					   
				}
								
    			this->bld_diffus = (double) (epicsTimeDiffInSeconds( &this->bld_recv_time, &this->previous_bld_time ) * 1000000.);

				this->previous_bld_time = this->bld_recv_time;	

    			if (this->bld_diffus > this->bld_diffus_max) 	this->bld_diffus_max = this->bld_diffus;
    			if (this->bld_diffus < this->bld_diffus_min) 	this->bld_diffus_min = this->bld_diffus;	
				this->bld_diffus_avg = (this->bld_diffus + this->bld_diffus_avg) / 2.0;		
				
				if 	(this->bld_diffus > (1.0/120)*1000000.) this->late_bld_pulse++;	
				
				if (this->last_bld_pulse+3 != header->fiducialId) { /* received correct pulseID */ 
					if (this->last_bld_pulse==131037) {
						if (header->fiducialId == 0) { /* pulse ID rollover? */  }
					}
					else {
						this->miss_bld_pulse +=  abs(header->fiducialId - this->last_bld_pulse) / 3;
					}
				}
				
				this->last_bld_pulse = header->fiducialId;
				
				/* check timestamp against EVG timestamp */	
				/* if  header->fiducialId not the last pulseID, check if within the past 2800 EVG PULSEIDs */

				/* Get EVG timestamp for header->fiducialId */	
				epicsTsPulseId = getTimeStamp(header->fiducialId);

				if (epicsTsPulseId.secPastEpoch)  { /* if  header->fiducialId within the past 2800 EVG PULSEIDs */
					header->tv_sec = epicsTsPulseId.secPastEpoch;
					header->tv_nsec = epicsTsPulseId.nsec;
				}	
				else { /* missed pulse - no match within last 2800 pulseIds*/
					this->miss_bld_pulse++;			/* SCAN will take be done on the original timestamps in BLD packet */
													/* this may mess up the BSA buffer */
				}					

        	epicsMutexUnlock(this->mutex);						
			
  	  		if (strcmp(this->multicast_group,"239.255.24.1")== 0)  {
			   scanIoRequest(bldPhaseCavityIoscan);
			}
  	  		else if (strcmp(this->multicast_group,"239.255.24.4")== 0)   {  
			   scanIoRequest(bldHxxUm6Imb01Ioscan);
			}
  	  		else if (strcmp(this->multicast_group,"239.255.24.5")== 0) { 
			   scanIoRequest(bldHxxUm6Imb02Ioscan); 
			}	
			
		}
		/* if a BLD packet corresponding to a pulseID never arrived, then currently there is no logic to detect this
		   and fill with NaN data. BSA buffer will be messed up currently. Add NaN data in future for missed BLDs. */	
	}
}

BLDMCastReceiver *bldPhaseCavityReceiver = NULL;
BLDMCastReceiver *bldHxxUm6Imb01Receiver = NULL;
BLDMCastReceiver *bldHxxUm6Imb02Receiver = NULL;

void bld_receivers_report(int level) {
  if (bldPhaseCavityReceiver != NULL) {
    phase_cavity_report(bldPhaseCavityReceiver, level);
  }
  if (bldHxxUm6Imb01Receiver != NULL) {
    imb_report(bldHxxUm6Imb01Receiver, level);
  } 
  if (bldHxxUm6Imb02Receiver != NULL) {
    imb_report(bldHxxUm6Imb02Receiver, level);
  }    
}

/* ==========================================================================

    Auth: Shantha Condamoor
    Date: 24-Jun-2014

    Name: fid_process_install and bldFidProcess

    Comments: Called during bld_hook_init() - before iocInit - to initialize fiducial data processing

============================================================================= */
static void bldFidProcess(void *unused)
{
	/* if ( ( fidProcessGeneric( 0, bldFidProcessActiveTimeslots ) & bldFidProcessActiveTimeslots ) ) {	 */
		/* active timeslot; resume do rocessing task(s) */
		int         rc;
		epicsTimeStamp epicsTs;
		
		rc = evrTimeGetFromPipeline(&epicsTs, evrTimeActive, 0, 0, 0, 0, 0);
		
		Bld_Pulseid_Timestamp[Bld_Pulseid_Count] = epicsTs;
		Bld_Pulseid[Bld_Pulseid_Count++] = PULSEID(epicsTs);

		if (2800 == Bld_Pulseid_Count) Bld_Pulseid_Count = 0;
	/* } */
}

static void bld_fid_process_install(void)
{
    /* fidProcessHasBeam = 1; */
	evrTimeRegister((FIDUCIALFUNCTION) bldFidProcess, 0);
	printf("BLD FID PROCESS INSTALL\n");
}

void showPulseId(void)
{
    int i;
	
    for (i=0; i<2800; i++) printf("%d: %d %d\n",Bld_Pulseid[i],Bld_Pulseid_Timestamp[i].secPastEpoch,Bld_Pulseid_Timestamp[i].nsec);
}

 /* ==========================================================================

    Auth: Shantha Condamoor
    Date: 24-Jun-2014

    Name: bld_receivers_start()

    Comments: Create all BLD Multicast receivers and start the tasks.
			Epics Message Queue removed per T.Straumann's suggestion.
			Now each each task's bld_receiver_run() processes the received BLD data and
			passes it directly to device support devBLDMCastReceiver via scanIoRequest.

============================================================================= */
void bld_receivers_start() {

  bld_fid_process_install();
  
  printf("\nINFO: Creating PhaseCavity Receiver... \n");
  phase_cavity_create(&bldPhaseCavityReceiver,BLD_PhaseCavity_GROUP);
  
  printf("\nINFO: Creating XRT HxxUm6Imb01Imb  Imb Receiver... \n");
  imb_create(&bldHxxUm6Imb01Receiver,BLD_HxxUm6Imb01_GROUP);  

  printf("\nINFO: Creating XRT HxxUm6Imb02Imb Receiver... \n");
  imb_create(&bldHxxUm6Imb02Receiver,BLD_HxxUm6Imb02_GROUP);  
  
  printf("\nINFO: Starting PhaseCavity Receiver... \n");

  epicsThreadMustCreate("BLDPhaseCavityProd",  epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bld_receiver_run, bldPhaseCavityReceiver);
  printf(" done.\n ");
  
  printf("\nINFO: Starting XRT Imb Receiver HxxUm6Imb01... \n");

  epicsThreadMustCreate("BLDHxxUm6Imb01Prod", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bld_receiver_run, bldHxxUm6Imb01Receiver);
  printf(" done.\n ");  
  
  printf("\nINFO: Starting XRT Imb Receiver HxxUm6Imb02... \n");

  epicsThreadMustCreate("BLDHxxUm6Imb02Prod", epicsThreadPriorityMedium, 20480,
			(EPICSTHREADFUNC)bld_receiver_run, bldHxxUm6Imb02Receiver);
  printf(" done.\n ");   
}

void bld_receiver_report(void *this, int level) {
  BLDMCastReceiver *receiver = this;
  
  if (receiver != NULL) {
    epicsMutexLock(receiver->mutex);

    printf("Received BLD packets : %ld\n", receiver->packets_received);
    printf("Processed BLD packets: %ld\n", receiver->packets_processed);
    printf("Received Late BLD packets : %ld\n", receiver->late_bld_pulse);
    printf("Missed BLD packets : %ld\n", receiver->miss_bld_pulse);	
    
    epicsMutexUnlock(receiver->mutex);
  } 

}

/* =============================================================================
   Auth: Shantha Condamoor
   Date: 24-Jun-2014
   Comments:
         The subroutine subTimeStampOfPulseId() has been replaced with equivalent
		 fiducial processing handler function bld_fid_process_install()
		 but left here in code just for reference
============================================================================= */
#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

long subTimeStampOfPulseId(struct subRecord *psub) {
  
  Bld_Pulseid_Timestamp[Bld_Pulseid_Count] = psub->time;
  psub->val = psub->a;
  Bld_Pulseid[Bld_Pulseid_Count++] = (Uint32_LE) psub->val;
  
  if (2800 == Bld_Pulseid_Count) Bld_Pulseid_Count = 0;
  
  return 0;
}

epicsRegisterFunction(subTimeStampOfPulseId);


