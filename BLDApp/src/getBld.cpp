#include <iostream>
#include <sstream>
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

#define SERVER_PORT 	10148

/* #define NCHANNELS	4 */

#define HxxUm6Imb01	-268494844		/* 239.255.24.4 */
#define Nh2Sb1Ipm01	-268494845		/* 239.255.24.3 */
#define FEEGasDetEnergy	-268494846		/* 239.255.24.2 */	
#define PhaseCavity	-268494847		/* 239.255.24.1 */

/**
 * Imb Data: all IMB share the same data type
 
> XRT:
>
> HxxUm6Imb01,
> HxxUm6Imb02,
> HfxDg2Imb01,
> HfxDg2Imb02,
> XcsDg3Imb03,
> XcsDg3Imb04,
> HfxDg3Imb01,
> HfxDg3Imb02,
> HfxMonImb01,
> HfxMonImb02,
> HfxMonImb03
>
> CXI Local:
>
> CxiDg1Imb01,
> CxiDg2Imb01,
> CxiDg2Imb02,
> CxiDg4Imb01
*/

/* Imb data type: */

namespace Pds {
    namespace Ipimb {
    
	#pragma pack(push,4)
	
	//IPIMB Configuration
	class ConfigV2 {
	    public:
	       uint64_t		triggerCounter;
	       uint64_t		serialID;
	       uint16_t		chargeAmpRange;
	       uint16_t		calibrationRange;
	       uint32_t		resetLength;
	       uint32_t		resetDelay;
	       float		chargeAmpRefVoltage;
	       float		calibrationVoltage;
	       float		diodeBias;
	       uint16_t		status;
	       uint16_t		errors;
	       uint16_t		calStrobeLength;
	       uint16_t		pad0;
	       uint32_t		trigDelay;
	       uint32_t		trigPsDelay;
	       uint32_t		adcDelay;       
	};

	// IPIMB Raw data
	class DataV2 {	  
	    public: 
	       uint64_t      triggerCounter;
	       uint16_t      config0;
	       uint16_t      config1;
	       uint16_t      config2;
	       uint16_t      channel0;      //< Raw counts value returned from channel 0.
	       uint16_t      channel1;      //< Raw counts value returned from channel 1.
	       uint16_t      channel2;      //< Raw counts value returned from channel 2.
	       uint16_t      channel3;      //< Raw counts value returned from channel 3. 
	       uint16_t      channel0ps;    //< Raw counts value returned from channel 0. 
	       uint16_t      channel1ps;    //< Raw counts value returned from channel 1.
	       uint16_t      channel2ps;    //< Raw counts value returned from channel 2.
	       uint16_t      channel3ps;    //< Raw counts value returned from channel 3.
	       uint16_t      checksum;
	};
		
	
    	#pragma pack(pop)
	
    } // namespace Ipimb
} // namespace Pds

namespace Pds {
    namespace Lusi {
    
    	// Processed results
	class IpmFexV1 {
	    public:
	       enum	{ NCHANNELS = 4 };
	       float	channel[NCHANNELS];
	       float	sum;
	       float	xpos;
	       float	ypos;
	};
    } // namespace Lusi
} // namespace Pds

namespace Pds {
    namespace Bld {
    	
	class BLDHeader {
	    public:	
		uint32_t tv_sec;
		uint32_t tv_nsec;
		uint32_t mbz1; /* must be zero */
		uint32_t fiducialId;
		uint32_t mbz2; /* must be zero */

	       /* Xtc Section 1 */
		uint32_t damage;
		uint32_t logicalId; /* source 1 */
		uint32_t physicalId; /* source 2 */
		uint32_t dataType; /* Contains - this is the version field */
		uint32_t extentSize; /* Extent - size of data following Xtc Section 2 */

	       /* Xtc Section 2 */
		uint32_t damage2;
		uint32_t logicalId2;
		uint32_t physicalId2;
		uint32_t dataType2;
		uint32_t extentSize2; /* Extent - size of data following Xtc Section 2 */

	        void report(void);	       
	};	    
	    
	class BldDataIpimbV1 {
	    public:
	       Ipimb::DataV2	ipimbData;
	       Ipimb::ConfigV2	ipimbConfig;
	       Lusi::IpmFexV1	ipmFexData;  
	       
	       void report(void);
	};
	
	class BldDataPCavV1 {
	    public:
	    	double fitTime1;	// in pico-seconds
	    	double fitTime2;	// in pico-seconds
	    	double charge1;		// in pico-columbs
	    	double charge2;		// in pico-columbs 
	       
	       void report(void);
	};	

    } // namespace Bld
    
} // namespace Pds

std::string addressToStr( unsigned int uAddr )  {
    unsigned int uNetworkAddr = htonl(uAddr);
    const unsigned char* pcAddr = (const unsigned char*) &uNetworkAddr;
    std::stringstream sstream;
    sstream << 
        (int) pcAddr[0] << "." <<
        (int) pcAddr[1] << "." <<
        (int) pcAddr[2] << "." <<
        (int) pcAddr[3];

    return sstream.str();
}

/*struct msghdr		include/linux/socket.h

struct iovec
{
       void                   *iov_base;
       __kernel_size_t        iov_len;
};

struct msghdr {
       void                  *msg_name;	// not really a name, but a pointer to a sockaddr_in structure, 
       					// which contains an IP address and a port number
       int                   msg_namelen;	//describes the length of this sockaddr_in structure
       struct iovec          *msg_iov;	//  refers to an array of iovec structures, which reference the payload
       					// payload can be present in a series of individual blocks, where each block is denoted 
					// in an iovec structure by its initial address (iov_base) and its length (iov_len)
       __kernel_size_t       msg_iovlen;	// can be used to pass protocol-specific control messages
       void                  *msg_control;
       __kernel_size_t       msg_controllen;
       unsigned              msg_flags;		// used to pass different flags both from the user process to the kernel and in the opposite direction
};
*/


class receiver {
public:
  receiver(unsigned uAddr, unsigned uPort, unsigned int uMaxDataSize,
	   char* sInterfaceIp = NULL);
  ~receiver();

  bool connect();
  bool run(int packets);
protected:
  int _sock;
  unsigned int      _uMaxDataSize;
  unsigned int      _uAddr, _uPort;
  bool              _bInitialized;
  char *_sInterfaceIp;
  
  struct msghdr      _hdr; // Control structure socket receive
  struct iovec       _iov[2]; // Buffer description socket receive - bld hdr, bld data, bld spare
  struct sockaddr_in _src; // Socket name source machine
};

receiver::receiver(unsigned uAddr, unsigned uPort, unsigned int uMaxDataSize,
		   char* sInterfaceIp) :
  _sock(-1),
  _uMaxDataSize(uMaxDataSize),
  _uAddr(uAddr), 
  _uPort(uPort),
  _bInitialized(false),
  _sInterfaceIp(sInterfaceIp) {
}

receiver::~receiver() {
}

bool receiver::connect() {
  _sock = socket(AF_INET, SOCK_DGRAM, 0);
  if ( _sock == -1 ) {
    printf("ERROR: socket()\n");
    return false;
  }
  
  int iRecvBufSize = _uMaxDataSize + sizeof(struct sockaddr_in);
#ifdef __linux__
  iRecvBufSize += 2048; // 1125
#endif

  if(setsockopt(_sock, SOL_SOCKET, SO_RCVBUF,
		(char*)&iRecvBufSize, sizeof(iRecvBufSize)) == -1) {
    printf("ERROR: setsockopt(SO_RECVBUF)\n");
    return false;
  }

  int iYes = 1;
  
  if(setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&iYes, sizeof(iYes)) == -1) {
    printf("ERROR: setsockopt(SO_REUSEADDR)\n");
    return false;
  }

  sockaddr_in sockaddrSrc;
  sockaddrSrc.sin_family      = AF_INET;
  sockaddrSrc.sin_addr.s_addr = htonl(_uAddr);
  sockaddrSrc.sin_port        = htons(_uPort);

  printf("Address=%d Port=%d\n", _uAddr, _uPort);
  
  if (bind( _sock, (struct sockaddr*) &sockaddrSrc, sizeof(sockaddrSrc) ) == -1 ) {
    printf("ERROR: bind\n");
    return false;
  }
    
  sockaddr_in sockaddrName;
  unsigned iLength = sizeof(sockaddrName);

  if(getsockname(_sock, (sockaddr*)&sockaddrName, &iLength) == 0) {
    unsigned int uSockAddr = ntohl(sockaddrName.sin_addr.s_addr);
    unsigned int uSockPort = (unsigned int )ntohs(sockaddrName.sin_port);
    printf( "Server addr: %s  Port %u  Buffer Size %u\n", addressToStr(uSockAddr).c_str(), uSockPort, iRecvBufSize );
  }
  else {
    printf("ERROR: getsockname\n");
    return false;
  }

  memset((void*)&_hdr, 0, sizeof(_hdr));
  _hdr.msg_name       = (caddr_t)&_src;
  _hdr.msg_namelen    = sizeof(_src);
  _hdr.msg_iov        = _iov;
  _hdr.msg_iovlen     = 2;	/* bld hdr, bld data, bld spare */
  
  in_addr_t uiInterface;

  if (_sInterfaceIp == NULL || _sInterfaceIp[0] == 0 ) {
    uiInterface = INADDR_ANY;
  }
  else {
    if ( _sInterfaceIp[0] < '0' || _sInterfaceIp[0] > '9' ) {
      struct ifreq ifr;
      strcpy( ifr.ifr_name, _sInterfaceIp );
      int iError = ioctl( _sock, SIOCGIFADDR, (char*)&ifr );
      if ( iError == 0 ) {
	uiInterface = ntohl( *(unsigned int*) &(ifr.ifr_addr.sa_data[2]) );
      }
      else {
	printf( "Cannot get IP address from network interface %s\n", _sInterfaceIp );
	uiInterface = 0;
      }
    }
    else {
      uiInterface = ntohl(inet_addr(_sInterfaceIp));
    }              
    
    if ( uiInterface != 0 ) {
      printf( "multicast interface IP: %s, inet_addr=%d\n", addressToStr(uiInterface).c_str(),htonl(_uAddr));
      
      struct ip_mreq ipMreq;
      memset((char*)&ipMreq, 0, sizeof(ipMreq));
      ipMreq.imr_multiaddr.s_addr = htonl(_uAddr);
      ipMreq.imr_interface.s_addr = htonl(uiInterface);
      if (setsockopt (_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&ipMreq,
			sizeof(ipMreq)) < 0 ) {
	printf("ERROR: setsockopt(IP_ADD_MEMBERSHIP)\n");
	return false;
      }
    }
    
    _bInitialized = true;    
   
  }
  return true;
}

bool receiver::run(int packets) {
  Pds::Bld::BLDHeader		bldHdr;
  
  Pds::Bld::BldDataIpimbV1	imbData;
  
  Pds::Bld::BldDataPCavV1	pcavData;

  int dupCnt = 0;
  int zeroCnt = 0;

  bool first = true;
  unsigned int prev_pulseid = 0xFFFF;

  int count = 0;
  while (count <= packets) {
    unsigned int iRecvDataSize = 0;

    if ( !_bInitialized ) {
      printf( "BldServerSlim::fetch() : BldServerSlim is not initialized successfully\n" );
      return false;
    }
  
    _iov[0].iov_base = &bldHdr;
    _iov[0].iov_len  = sizeof(bldHdr);       
    
    if (HxxUm6Imb01 == (int) _uAddr) {
	_iov[1].iov_base = &imbData;
	_iov[1].iov_len  = sizeof(imbData);	
    } 
    
    if (PhaseCavity == (int) _uAddr) {
	_iov[1].iov_base = &pcavData;
	_iov[1].iov_len  = sizeof(pcavData);   	
    }              

    int iFlags = 0;
    iRecvDataSize = recvmsg(_sock, &_hdr, iFlags); 
            
    if ( iRecvDataSize <= 0 ) {
      printf( "bldServerTest:main(): bldServer.fetch() failed. Error Code = %d\n", errno);
      return false;
    }
    else {
    
     	bldHdr.report();     
      
	if (HxxUm6Imb01 == _uAddr) {	/* HxxUm6Imb01 - 239.255.24.4 */	
     	   imbData.report();
	 } 

	 /* 268494845: Nh2Sb1Ipm01 -  239.255.24.3 */
	 /* 268494846: FEEGasDetEnergy - 239.255.24.2 */

	 if (PhaseCavity == _uAddr) {	/* PhaseCavity - 239.255.24.1 */
     	   pcavData.report();
	 }

	 std::cout << std::dec << count << ") 0x" << std::hex << bldHdr.fiducialId 
		   << std::dec << " " << bldHdr.fiducialId << " " << addressToStr( _uAddr ) << std::endl;

	 if (first) {
	   prev_pulseid = bldHdr.fiducialId;
	   first = false;
	 }
	 else {
	   if (prev_pulseid == bldHdr.fiducialId) {
	     dupCnt++;
	     std::cout << "dup" << std::endl;
	   }
	 }
	 if (bldHdr.fiducialId==0) {
	   zeroCnt++;
	 }
	 prev_pulseid = bldHdr.fiducialId;

	 count++;
    }
  }

  std::cout << "Duplicates: " << dupCnt << std::endl;
  std::cout << "Zeros:      " << zeroCnt << std::endl;
  return true;
}

void Pds::Bld::BLDHeader::report(void) {
		
	std::cout << "****************** BLD Header ****************** " << std::endl;
	std::cout << "tv_sec = 0x" << std::hex << tv_sec << std::endl;
	std::cout << "tv_nsec = 0x" << std::hex << tv_nsec << std::endl;
	std::cout << "mbz1 = 0x" << std::hex << mbz1 << std::endl; /* must be zero */
	std::cout << "fiducialId = 0x" << std::hex << fiducialId << std::endl;
	std::cout << "mbz2 = 0x" << std::hex << mbz2 << std::endl; /* must be zero */
  
  	/* Xtc Section 1 */
	std::cout << "damage = 0x" << std::hex << damage << std::endl;
	std::cout << "logicalId = 0x" << std::hex << logicalId << std::endl; /* source 1 */
	std::cout << "physicalId = 0x" << std::hex << physicalId << std::endl; /* source 2 */
	std::cout << "dataType = 0x" << std::hex << dataType << std::endl; /* Contains - this is the version field */
	std::cout << "extentSize = 0x" << std::hex << extentSize << std::endl; /* Extent - size of data following Xtc Section 2 */
  
  	/* Xtc Section 2 */
	std::cout << "damage2 = 0x" << std::hex << damage2 << std::endl;
	std::cout << "logicalId2 = 0x" << std::hex << logicalId2 << std::endl;
	std::cout << "physicalId2 = 0x" << std::hex << physicalId2 << std::endl;
	std::cout << "dataType2 = 0x" << std::hex << dataType2 << std::endl;
	std::cout << "extentSize2 = 0x" << std::hex << extentSize2 << std::endl; /* Extent - size of data following Xtc Section 2 */    			
}

void Pds::Bld::BldDataPCavV1::report(void) {
	std::cout << "****************** BLD Payload ****************** " << std::endl;
	std::cout << "fitTime1 = " << std::dec << fitTime1 << std::endl;
	std::cout << "fitTime2 = " << std::dec << fitTime2 << std::endl;	
	std::cout << "charge1 = " << std::dec << charge1 << std::endl;	
	std::cout << "charge2 = " << std::dec << charge2 << std::endl;								
	std::cout << "********************************************* " << std::endl;		
}				

void Pds::Bld::BldDataIpimbV1::report(void) {

	std::cout << "****************** BLD Payload ****************** " << std::endl;
	std::cout << "triggerCounter = " << std::dec << ipimbData.triggerCounter << std::endl;
	std::cout << "config0 = " << std::dec << ipimbData.config0 << std::endl;
	std::cout << "config1 = " << std::dec << ipimbData.config1 << std::endl;
	std::cout << "config2 = " << std::dec << ipimbData.config2 << std::endl;
	std::cout << "channel0 = " << std::dec << ipimbData.channel0 << std::endl;	/**< Raw counts value returned from channel 0. */
	std::cout << "channel1 = " << std::dec << ipimbData.channel1 << std::endl;	/**< Raw counts value returned from channel 1. */
	std::cout << "channel2 = " << std::dec << ipimbData.channel2 << std::endl;	/**< Raw counts value returned from channel 2. */
	std::cout << "channel3 = " << std::dec << ipimbData.channel3 << std::endl;	/**< Raw counts value returned from channel 3. */
	std::cout << "channel0ps = " << std::dec << ipimbData.channel0ps << std::endl;	/**< Raw counts value returned from channel 0. */
	std::cout << "channel1ps = " << std::dec << ipimbData.channel1ps << std::endl;	/**< Raw counts value returned from channel 1. */
	std::cout << "channel2ps = " << std::dec << ipimbData.channel2ps << std::endl;	/**< Raw counts value returned from channel 2. */
	std::cout << "channel3ps = " << std::dec << ipimbData.channel3ps << std::endl;	/**< Raw counts value returned from channel 3. */
	std::cout << "checksum = " << std::dec << ipimbData.checksum << std::endl;

/*  IPIMB Configuration */
	std::cout << "triggerCounterConfig = " << std::dec << ipimbConfig.triggerCounter << std::endl;   
	std::cout << "serialID = " << std::dec << ipimbConfig.serialID << std::endl;
	std::cout << "chargeAmpRange = " << std::dec << ipimbConfig.chargeAmpRange << std::endl;
	std::cout << "calibrationRange = " << std::dec << ipimbConfig.calibrationRange << std::endl;
	std::cout << "resetLength = " << std::dec << ipimbConfig.resetLength << std::endl;
	std::cout << "resetDelay = " << std::dec << ipimbConfig.resetDelay << std::endl;
	std::cout << "chargeAmpRefVoltage = " << std::dec << ipimbConfig.chargeAmpRefVoltage << std::endl;
	std::cout << "calibrationVoltage = " << std::dec << ipimbConfig.calibrationVoltage << std::endl;
	std::cout << "diodeBias = " << std::dec << ipimbConfig.diodeBias << std::endl;
	std::cout << "status = " << std::dec << ipimbConfig.status << std::endl;
	std::cout << "errors = " << std::dec << ipimbConfig.errors << std::endl;
	std::cout << "calStrobeLength = " << std::dec << ipimbConfig.calStrobeLength << std::endl;
	std::cout << "pad0 = " << std::dec << ipimbConfig.pad0 << std::endl;
	std::cout << "trigDelay = " << std::dec << ipimbConfig.trigDelay << std::endl;
	std::cout << "trigPsDelay = " << std::dec << ipimbConfig.trigPsDelay << std::endl;
	std::cout << "adcDelay = " << std::dec << ipimbConfig.adcDelay << std::endl;			

	std::cout << "channel 0 = " << std::dec << ipmFexData.channel[0] << std::endl;
	std::cout << "channel 1 = " << std::dec << ipmFexData.channel[1] << std::endl;				
	std::cout << "channel 2 = " << std::dec << ipmFexData.channel[2] << std::endl;
	std::cout << "channel 3 = " << std::dec << ipmFexData.channel[3] << std::endl;			
	std::cout << "sum = " << std::dec << ipmFexData.sum << std::endl;			
	std::cout << "xpos = " << std::dec << ipmFexData.xpos << std::endl;	
	std::cout << "ypos = " << std::dec << ipmFexData.ypos << std::endl;	
	std::cout << "********************************************* " << std::endl;	
}

static void
usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-h] [-d level] [-p port] [-d bld_pkts] multicast IP\n", nm);
	fprintf(stderr,"       -h:         this message\n");
	fprintf(stderr,"       -d bld_pkts:   Number of BLD packets to receive\n\n");
	fprintf(stderr,"       -p port:    port on which to listen (default: %u)\n", SERVER_PORT);
	fprintf(stderr,"       -e eth:     Ethernet interface (default: eth0)\n");
}

int main(int argc, char** argv) {

    int  ch;
    int bld_pkts = 0;
    unsigned short port = SERVER_PORT;
    char eth[]  = "eth0";    
    
    while ( (ch = getopt(argc, argv, "d:hp:e")) >= 0 ) {
	    switch (ch) {
		    case 'e':
			    if ( 1 != sscanf(optarg,"%s",&eth) ) {
				    fprintf(stderr,"Unable to scan argument to '-e' (short int)\n");
				    return 1;
			    }
			    break;	    

		    case 'p':
			    if ( 1 != sscanf(optarg,"%hi",&port) ) {
				    fprintf(stderr,"Unable to scan argument to '-p' (short int)\n");
				    return 1;
			    }
			    break;

		    case 'h':
			    usage(argv[0]);
			    return 0;

		    case 'd':
			    if ( 1 != sscanf(optarg,"%i",&bld_pkts) ) {
				    fprintf(stderr,"Unable to scan argument to '-d' (int)\n");
				    return 1;
			    }
			    break;
	    }
    }

    /** counts the number of arguments to make sure they are valid */
    if(optind >= argc){
	    fprintf(stderr, "error: argument needed to identify device\n");
	    return -1;
    }
    
    printf("argc %d optind %d %s %s \n",argc,optind,argv[optind],argv[optind+1]);
	
  // PCAV address *.1
  //  receiver r(ntohl(inet_addr("239.255.24.1")), port, 1024, "eth0");

    pid_t childPID[argc-optind];
	
	for (int i=0; i < argc-optind; i++ ) {
	
    	if ((childPID[i] = fork()) >= 0) { // fork successful

        	if(childPID[i] == 0) { // child process

            	printf("\n Child Process for receiver %d\n",i);	

  	  			receiver r(ntohl(inet_addr(argv[optind+i])), port, 1024, eth);

  	  			if (!r.connect()) {
					fprintf(stderr, "ERROR: unable to connect to %s\n",argv[optind+i]);
				}
				else {				
  	    			if (!r.run(bld_pkts)) {
    					fprintf(stderr, "ERROR: Child Process for receiver %d: Failed to run",i);   		
  	    			}	
  	  			}					  
	  		}
		}
    	else { // fork failed
        	printf("\n Fork failed for r1, quitting!!!!!!\n");
    	}
				
	}
  
  return 0;
}

