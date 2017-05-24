/* $Id: BLDMCastReceiver.c,v 1.21 2015/04/01 16:39:03 scondam Exp $ */
/*=============================================================================

  Name: BLDMCastReceiver.c

  Abs:  BLDMCast Receiver driver for data sent from PCD.

  Auth: Luciano Piccoli (lpiccoli)
  Mod:	Shantha Condamoor (scondam)

  Mod:  24-May-2013 - L.Piccoli	- BLD-R2-0-0-BR - partial implementation of bld receiver
		11-Jun-2013 - L.Piccoli	- BLD-R2-3-0, BLD-R2-2-0 - Addition of BLD receiver - phase cavity
		27-Feb-2014 - L.Piccoli - BLD-R2-4-1, BLD-R2-4-0  - Merged back R2-0-0-BR
		3-Mar-2014  - L.Piccoli - BLD-R2-4-2 - Fixed wrong BLD IP address in multicast BLD receiver - dbior bug fix
		6-Mar-2014 -  L.Piccoli - BLD-R2-4-5 - BLD-R2-4-4, BLD-R2-4-3 - adding code for preventing multicast if host is not ioc-sys0-bd01 or ioc-b34-bd01
		12-Mar-2014 - L.Piccoli - BLD-R2-4-6 - Addition of code that prevents BLDCast task from being spawned if the code is not running on ioc-sys0-bd01
		28-Apr-2014 - S.Condamoor - BLD-R2-5-0 - IMB support added
		27-May-2014 - S.Condamoor - BLD-R2-5-1 - More Diagnostics for BLD Receiver Packets. Release BLD-R2-5-1
		28-May-2014 - S.Condamoor - BLD-R2-5-2 - Get timestamp from EVR
		10-Jun-2014 - S.Condamoor - BLD-R2-5-3  - Added support for multiple IMB devices;
		17-Jun-2014 - S.Condamoor - BLD-R2-5-4  - Split Sender and Receiver apps
		7-Jul-2014  - S.Condamoor - BLD-R2-6-0 - Message Queue removed. Use Double buffering scheme per T.Straumann's recommendation.
												Fiducial Processing added for obtaining timestamps for matching pulseids.
												nsec/sec fields swapped.
												Code for ignoring duplicate BLDs, counting lost and late BLD packets added.
		4-May-2015 - S.Condamoor: - BLD-R2-6-5 - Added support for FEEGasDetEnergyReceiver													
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>

#ifdef RTEMS
#include <bsp/gt_timer.h>
#endif

#include <epicsThread.h>
#include <epicsEvent.h>
#include <initHooks.h>

#include <errlog.h>

#include "evrTime.h"
#include "evrPattern.h"
#include "devBusMapped.h"

#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"
#include "BLDMCastReceiverImb.h"
#include "BLDMCastReceiverGdet.h"

/* node			ipnum			name				network_name	ethernet			dhcp
-------------	-------------	------------------	------------	-----------------	----
PROD
-----
IOC-SYS0-BD02	172.27.29.100	IOC-SYS0-BD02-FNET	LCLSFNET		00:01:af:2c:6d:f4	Yes
"				172.27.2.203	IOC-SYS0-BD02		LCLSIOC			00:01:af:2c:6d:f3	"
"				172.27.225.22	IOC-SYS0-BD02-BLD	LCLSBLD			-					No
DEV
-----
IOC-B34-BD02	172.25.160.76	IOC-B34-BD02-BLD	B034-LCLSBLD	-					No
"				172.25.160.29	IOC-B34-BD02-FNET	B034-LCLSFBCK	"					"
"				134.79.218.187	IOC-B34-BD02		LCLSDEV			"					"
----
*/
/* scondam: 8-Apr-2015: Separate port and subnet for BLD traffic */

/* ioc-sys0-bd01 and ioc-b34-bd01 */
#define BLD_PROD_SNDR_IOC1_ETH0 "172.27.2.162"
#define BLD_DEV_SNDR_IOC1_ETH0 "134.79.219.145"

/* ioc-sys0-bd02 and ioc-b34-bd02 */
#define BLD_PROD_RCVR_IOC1_ETH0 "172.27.2.203"
#define BLD_DEV_RCVR_IOC1_ETH0 "134.79.219.145"
/* ioc-sys0-bd02-fnet and ioc-b34-bd02-fnet */
#define BLD_PROD_RCVR_IOC1_ETH1 "172.27.29.100"
#define BLD_DEV_RCVR_IOC1_ETH1 "172.25.160.23"
/* ioc-sys0-bd02-bld and ioc-b34-bd02-bld */
#define BLD_PROD_RCVR_IOC1_ETH2 "172.27.225.22"
#define BLD_DEV_RCVR_IOC1_ETH2 "172.25.160.75"

/* Global BLD Receiver enables */
int ENABLE_GDET_RECEIVER	= 1;
int ENABLE_IMB_RECEIVER		= 0;
int ENABLE_PCAV_RECEIVER	= 0;

extern IOSCANPVT bldPhaseCavityIoscan;
extern IOSCANPVT bldFEEGasDetEnergyIoscan;
extern IOSCANPVT bldHxxUm6Imb01Ioscan;
extern IOSCANPVT bldHxxUm6Imb02Ioscan;
/* scondam: 31-Mar-2015: Test BLD PCAV */
extern IOSCANPVT bldPhaseCavityTestIoscan;

typedef  int16_t  __attribute__ ((may_alias))  int16_t_a;
typedef uint32_t  __attribute__ ((may_alias)) uint32_t_a;

int				epicsTsDebug0[2800] = {0};
int				epicsTsDebug1[2800] = {0};
int				epicsTsDebug2[2800] = {0};
int				epicsTsDebug3[2800] = {0};
int				epicsTsDebug = 0;
int				epicsDebug = 0;

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
  
    printf("Hostname: %s Eth: %s\n",name,getenv("MCASTETHPORT"));
	
	if (strcmp("PROD_IPADDR0", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_PROD_RCVR_IOC1_ETH0;
	}	
	else if (strcmp("DEV_IPADDR0", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_DEV_RCVR_IOC1_ETH0;
	}		
	else if (strcmp("PROD_IPADDR1", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_PROD_RCVR_IOC1_ETH1;
	}
	else if (strcmp("DEV_IPADDR1", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_DEV_RCVR_IOC1_ETH1;
	}	
	else if (strcmp("PROD_IPADDR2", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_PROD_RCVR_IOC1_ETH2;
	}	
	else if (strcmp("DEV_IPADDR2", getenv("MCASTETHPORT")) == 0) {
		interface_string = BLD_DEV_RCVR_IOC1_ETH2;
	}		
	else {
	  	printf("ERROR: BLD code running on unknown ethernet Port\n");
	  	return -1;
	}
	
    printf("Hostname: %s Eth: %s Addr: %s\n",name,getenv("MCASTETHPORT"),interface_string);	
    
    /* if (strcmp("ioc-sys0-bd01", name) == 0) {
      interface_string = BLD_PROD_SNDR_IOC1_ETH0;
    }
    else {
      if (strcmp("ioc-b34-bd01", name) == 0) {
	interface_string = BLD_DEV_SNDR_IOC1_ETH0;
      }
      else {
		if (strcmp("ioc-sys0-bd02", name) == 0) {
			interface_string = BLD_PROD_RCVR_IOC1_ETH1;
		}
		else {
			if (strcmp("ioc-b34-bd01", name) == 0) {		
	  			interface_string = BLD_DEV_RCVR_IOC1_ETH1;				
			}
			else {
	  			printf("ERROR: BLD code running on unknown IOC\n");
	  			return -1;
			}
		}
      }
    } */
  }
  else {
    printf("ERROR: Unable to get hostname (errno=%d, rtncode=%d)\n", errno, rtncode);
    return -1;
  }

  printf("INFO: MCAST ETH address is %s\n", interface_string);

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
 * for the phase cavity BLD or size of 7 floats for Imb or 6 floats for gdet)
 * @param payload_count number of parameters in the BLD payload (e.g. 4
 * parameters for the phase cavity BLD or 7 parameters for Imb)
 * @param multicast_group BLD multicast group address in dotted string form
 * @param port socket port
 *
 * @return 0 on success, -1 on failure
 */
int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count,
			char *multicast_group, int port)
{
	*this = (BLDMCastReceiver *) malloc(sizeof(BLDMCastReceiver));
	if (*this == NULL) {
      fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
      return -1;
	}

	(*this)->bld_header_bsa = (BLDHeader *) malloc(sizeof(BLDHeader) + payload_size );
	if ((*this)->bld_header_bsa == NULL) {
      fprintf(stderr, "ERROR: Failed to allocate memory for BLDMCastReceiver\n");
      return -1;
	}
	(*this)->bld_payload_bsa = ((*this)->bld_header_bsa) + 1;

	(*this)->packets_received = 0;
	(*this)->packets_duplicates = 0;
	(*this)->miss_bld_pulse = 0;
	(*this)->payload_size = payload_size;
	(*this)->payload_count = payload_count;
	(*this)->multicast_group = multicast_group;
	(*this)->port = port;
    (*this)->bld_diffus = 8333;
    (*this)->bld_diffus_max = 8333;
    (*this)->bld_diffus_min = 8333;
    (*this)->bld_diffus_avg = 8333;

	if (strcmp(multicast_group,"239.255.24.1")== 0) {
		check("PhaseCavity_max_f", &(*this)->bld_diffus_max);
		check("PhaseCavity_min_f", &(*this)->bld_diffus_min);
		check("PhaseCavity_avg_f", &(*this)->bld_diffus_avg);
		check("PhaseCavity_dup_f", &(*this)->packets_duplicates);
		check("PhaseCavity_late_f", &(*this)->late_bld_pulse);
		check("PhaseCavity_miss_f", &(*this)->miss_bld_pulse);
	}
	if (strcmp(multicast_group,"239.255.24.2")== 0) {
		check("FEEGasDetEnergy_max_f", &(*this)->bld_diffus_max);
		check("FEEGasDetEnergy_min_f", &(*this)->bld_diffus_min);
		check("FEEGasDetEnergy_avg_f", &(*this)->bld_diffus_avg);
		check("FEEGasDetEnergy_dup_f", &(*this)->packets_duplicates);
		check("FEEGasDetEnergy_late_f", &(*this)->late_bld_pulse);
		check("FEEGasDetEnergy_miss_f", &(*this)->miss_bld_pulse);
	}	
	else if (strcmp(multicast_group,"239.255.24.4")== 0) {
		check("HxxUm6Imb01_max_f", &(*this)->bld_diffus_max);
		check("HxxUm6Imb01_min_f", &(*this)->bld_diffus_min);
		check("HxxUm6Imb01_avg_f", &(*this)->bld_diffus_avg);
		check("HxxUm6Imb01_dup_f", &(*this)->packets_duplicates);
		check("HxxUm6Imb01_late_f", &(*this)->late_bld_pulse);
		check("HxxUm6Imb01_miss_f", &(*this)->miss_bld_pulse);
	}
	else if (strcmp(multicast_group,"239.255.24.5")== 0) {
		check("HxxUm6Imb02_max_f", &(*this)->bld_diffus_max);
		check("HxxUm6Imb02_min_f", &(*this)->bld_diffus_min);
		check("HxxUm6Imb02_avg_f", &(*this)->bld_diffus_avg);
		check("HxxUm6Imb02_dup_f", &(*this)->packets_duplicates);
		check("HxxUm6Imb02_late_f", &(*this)->late_bld_pulse);
		check("HxxUm6Imb02_miss_f", &(*this)->miss_bld_pulse);
	}
	else if (strcmp(multicast_group,"239.255.24.254")== 0) {
		check("PhaseCavityTest_max_f", &(*this)->bld_diffus_max);
		check("PhaseCavityTest_min_f", &(*this)->bld_diffus_min);
		check("PhaseCavityTest_avg_f", &(*this)->bld_diffus_avg);
		check("PhaseCavityTest_dup_f", &(*this)->packets_duplicates);
		check("PhaseCavityTest_late_f", &(*this)->late_bld_pulse);
		check("PhaseCavityTest_miss_f", &(*this)->miss_bld_pulse);
	}

  #ifndef SIGNAL_TEST
	/** Create socket and register to multicast group */
	if (bld_register_mulitcast(*this) < 0) {
      free((*this)->bld_header_bsa);

      free(*this);
      *this = NULL;
      return -1;
	}
  #endif

	(*this)->mutex = epicsMutexMustCreate();

	return 0;
}

int bld_receiver_destroy(BLDMCastReceiver *this) {
  if (this == NULL) {
    return -1;
  }

  epicsMutexDestroy(this->mutex);

  free(this->bld_header_bsa);

  free(this);

  return -1;
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

  	size_t recvSize = 0;
  	struct msghdr msghdr;
  	int flags = 0;

  	struct iovec       iov; // Buffer description socket receive
  	struct sockaddr_in src; // Socket name source machine

	BLDHeader *rcv_buf = (BLDHeader *) malloc(sizeof(BLDHeader) + this->payload_size );
    void *payload_buf = rcv_buf+1;

	BLDHeader *header = NULL;
	BLDPhaseCavity *pcav1 = NULL;
	BLDGdet *gdet2 = NULL;	
	BLDImb *imb4 = NULL;
	BLDImb *imb5 = NULL;

	BLDPhaseCavity *pcav254 = NULL;

    printf("BLD_RECEIVER_RUN()\n");

    printf("INFO: Waiting for BLD packets from group %s\n", this->multicast_group);

  	epicsThreadSleep(5);

    /** Last BLD pulseid received */
    int last_bld_pulse = -1;
    /** Current BLD pulseid received */
    int curr_bld_pulse = -1;

	long late_bld = 0;  /* late BLD packets - increments by one for every timeslot missed */
	long miss_bld = 0;	/* missed BLD packets - increments by one if no match could be found in past 2800 EVG pulseids */

	epicsTimeStamp epicsTsPrevious;
    evrTimeGetFromPipeline(&epicsTsPrevious, evrTimeCurrent, 0, 0, 0, 0, 0);

	for (;;) {

		iov.iov_base = rcv_buf;
  		iov.iov_len  = sizeof(BLDHeader) + this ->payload_size ;

  		memset((void*)&msghdr, 0, sizeof(msghdr));
  		msghdr.msg_name = (caddr_t)&src;
  		msghdr.msg_namelen = sizeof(src);
  		msghdr.msg_iov = &iov;
  		msghdr.msg_iovlen = 1;

		/* initialize the receive buffer */
		recvSize = 0;

		/*   blocks on recvmsg() - Wait for BLD Multicast  */
  		recvSize = recvmsg(this->sock, &msghdr, flags);

  		if (recvSize > 0) {

				curr_bld_pulse = (int) __ld_le32(&(rcv_buf->fiducialId));

				/* check for duplicate packets */
				if (last_bld_pulse == curr_bld_pulse) {
					this->packets_duplicates++;
				}
				else {
        			/* BLD packet received time */
					epicsTimeStamp epicsTs;

		    		last_bld_pulse = curr_bld_pulse;

					if 	(curr_bld_pulse != PULSEID(epicsTs)) late_bld++;

        			epicsMutexMustLock(this->mutex);

		    			/* scan only if not a duplicate packet */
						/* double-buffer per T.Straumann's recommendation */
    					header = rcv_buf;

  	  					if (strcmp(this->multicast_group,"239.255.24.1")== 0)  {
						   pcav1 = (BLDPhaseCavity *) payload_buf;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.2")== 0)   {
						   gdet2 = (BLDGdet *) payload_buf;
						}						
  	  					else if (strcmp(this->multicast_group,"239.255.24.4")== 0)   {
						   imb4 = (BLDImb *) payload_buf;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.5")== 0) {
						   imb5 = (BLDImb *) payload_buf;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.254")== 0)  {
						   pcav254 = (BLDPhaseCavity *) payload_buf;
						}

						rcv_buf = this->bld_header_bsa;
						payload_buf = this->bld_payload_bsa;

						this->bld_header_bsa = header;

  	  					if (strcmp(this->multicast_group,"239.255.24.1")== 0)  {
						   this->bld_payload_bsa = pcav1;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.2")== 0)   {
						   this->bld_payload_bsa = gdet2;
						}						
  	  					else if (strcmp(this->multicast_group,"239.255.24.4")== 0)   {
						   this->bld_payload_bsa = imb4;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.5")== 0) {
						   this->bld_payload_bsa = imb5;
						}
  	  					else if (strcmp(this->multicast_group,"239.255.24.254")== 0)  {
						   this->bld_payload_bsa = pcav254;
						}

            			this->packets_received++;

                        evrTimeGetFromPipeline(&epicsTs, evrTimeCurrent, 0, 0, 0, 0, 0);
                        /* epicsTimeGetCurrent(&epicsTs); */
                        epicsTsDebug0[epicsTsDebug] = (int) __ld_le32(&(rcv_buf->tv_nsec));
                        epicsTsDebug1[epicsTsDebug] = (int) epicsTs.nsec;
                        epicsTsDebug2[epicsTsDebug] = curr_bld_pulse;
                        epicsTsDebug3[epicsTsDebug] = PULSEID(epicsTs);

    					this->bld_diffus = (long) (epicsTimeDiffInSeconds( &epicsTs, &epicsTsPrevious ) * 1000000);
                        epicsTsPrevious = epicsTs;

    					if (this->bld_diffus > this->bld_diffus_max) 	this->bld_diffus_max = this->bld_diffus;
    					if (this->bld_diffus < this->bld_diffus_min) 	this->bld_diffus_min = this->bld_diffus;
			    		this->bld_diffus_avg = (this->bld_diffus + this->bld_diffus_avg) / 2;

						this->miss_bld_pulse += miss_bld;
						this->late_bld_pulse += late_bld;

					epicsMutexUnlock(this->mutex);

                    if (strcmp(this->multicast_group,"239.255.24.1")== 0)  {
                        scanIoRequest(bldPhaseCavityIoscan);
                    }
                    else if (strcmp(this->multicast_group,"239.255.24.2")== 0)   {
                        scanIoRequest(bldFEEGasDetEnergyIoscan);
                    }					
                    else if (strcmp(this->multicast_group,"239.255.24.4")== 0)   {
                        scanIoRequest(bldHxxUm6Imb01Ioscan);
                    }
                    else if (strcmp(this->multicast_group,"239.255.24.5")== 0) {
                        scanIoRequest(bldHxxUm6Imb02Ioscan);
                    }
                    else if (strcmp(this->multicast_group,"239.255.24.254")== 0)  {
                        scanIoRequest(bldPhaseCavityTestIoscan);
                    }

					epicsTsDebug++;
					if (epicsTsDebug == 2800) epicsTsDebug = 0;

					if (epicsDebug > 0) {
						int k;
						for (k=0; k < epicsDebug; k++) printf("[%d] %d - {%d} %d\n",epicsTsDebug0[k],epicsTsDebug1[k],epicsTsDebug2[k],epicsTsDebug3[k]);
						epicsDebug = 0;
					}
				}

		}

		/* if a BLD packet corresponding to a pulseID never arrived, then currently there is no logic to detect this
		   and fill with NaN data. BSA buffer will be messed up currently. Add NaN data in future for missed BLDs. */
	}

	free(rcv_buf);
}

BLDMCastReceiver *bldPhaseCavityReceiver = NULL;
BLDMCastReceiver *bldFEEGasDetEnergyReceiver = NULL;
BLDMCastReceiver *bldHxxUm6Imb01Receiver = NULL;
BLDMCastReceiver *bldHxxUm6Imb02Receiver = NULL;
BLDMCastReceiver *bldPhaseCavityTestReceiver = NULL;

void bld_receivers_report(int level) {
  if (bldPhaseCavityReceiver != NULL) {
    phase_cavity_report(bldPhaseCavityReceiver, level);
  }
  if (bldFEEGasDetEnergyReceiver != NULL) {
    gdet_report(bldFEEGasDetEnergyReceiver, level);
  }  
  if (bldHxxUm6Imb01Receiver != NULL) {
    imb_report(bldHxxUm6Imb01Receiver, level);
  }
  if (bldHxxUm6Imb02Receiver != NULL) {
    imb_report(bldHxxUm6Imb02Receiver, level);
  }
  if (bldPhaseCavityTestReceiver != NULL) {
    phase_cavity_report(bldPhaseCavityTestReceiver, level);
  }
}

int bldReceiverThreadStart( const char * threadName, BLDMCastReceiver * pBLDReceiver )
{
	int	status = -1;
	if ( pBLDReceiver )
	{
		epicsThreadId	tid;
		printf("\nINFO: Starting %s BLDReceiver thread ... \n", threadName );
		tid = epicsThreadMustCreate(	threadName, epicsThreadPriorityHigh-1,
								epicsThreadGetStackSize(epicsThreadStackMedium),
								(EPICSTHREADFUNC) bld_receiver_run, pBLDReceiver	);
		if ( tid )
		{
			status = 0;
			printf("\nINFO: %s BLDReceiver thread Started.\n", threadName );
		}
		else
		{
			printf("\nERROR: Failed to start %s BLDReceiver thread.\n", threadName );
		}
	}
	return status;
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
void bld_receivers_start()
{
  if ( ENABLE_PCAV_RECEIVER ) {
	printf("\nINFO: Creating PhaseCavity Receiver... \n");
	phase_cavity_create(&bldPhaseCavityReceiver,BLD_PhaseCavity_GROUP);
	bldReceiverThreadStart( "BLDPhaseCavityProd",		bldPhaseCavityReceiver );
  }
  
  if ( ENABLE_GDET_RECEIVER ) {
	printf("\nINFO: Creating XRT  FEEGasDetEnergyReceiver... \n");
	gdet_create(&bldFEEGasDetEnergyReceiver,BLD_FEEGasDetEnergy_GROUP);  
    bldReceiverThreadStart( "BLDFEEGasDetEnergyProd", bldFEEGasDetEnergyReceiver );
  }

  if ( ENABLE_IMB_RECEIVER ) {
	printf("\nINFO: Creating XRT HxxUm6Imb01Imb  Imb Receiver... \n");
	imb_create(&bldHxxUm6Imb01Receiver,BLD_HxxUm6Imb01_GROUP);
    bldReceiverThreadStart( "BLDHxxUm6Imb01Prod",		bldHxxUm6Imb01Receiver );
  }

  if ( ENABLE_IMB_RECEIVER ) {
	printf("\nINFO: Creating XRT HxxUm6Imb02Imb Receiver... \n");
	imb_create(&bldHxxUm6Imb02Receiver,BLD_HxxUm6Imb02_GROUP);
    bldReceiverThreadStart( "BLDHxxUm6Imb02Prod",		bldHxxUm6Imb02Receiver );
  }

  if ( ENABLE_PCAV_RECEIVER ) {
	printf("\nINFO: Creating PhaseCavityTest Receiver... \n");
	phase_cavity_create(&bldPhaseCavityTestReceiver,BLD_PhaseCavityTest_GROUP);
    bldReceiverThreadStart( "BLDPhaseCavityTestProd", bldPhaseCavityTestReceiver );
  }

}

void bld_receiver_report(void *this, int level) {

  BLDMCastReceiver *receiver = this;
  
  if (receiver != NULL) {
    epicsMutexMustLock(receiver->mutex);

    printf("Received BLD packets : %ld\n", receiver->packets_received);
    printf("Processed BLD packets: %ld\n", receiver->packets_duplicates);
    printf("Received Late BLD packets : %ld\n", receiver->late_bld_pulse);
    printf("Missed BLD packets : %ld\n", receiver->miss_bld_pulse);

    epicsMutexUnlock(receiver->mutex);
  }

}
