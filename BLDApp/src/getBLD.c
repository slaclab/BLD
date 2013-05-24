#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
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

/* USAGE: getBLD("172.27.10.185") on ioc-sys0-fb05 */

static int _sock = 0;

static void addressToStr(unsigned int address, char *str) {
  unsigned int networkAddr = htonl(address);
  const unsigned char* pcAddr = (const unsigned char*) &networkAddr;

  sprintf(str, "%u.%u.%u.%u", pcAddr[0], pcAddr[1], pcAddr[2], pcAddr[3]);
}

static int createSocket(unsigned int address, unsigned int port, int recvBufSize) {
  _sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (_sock == -1) {
    printf("ERROR: Failed to create socket (errno=%d)\n", errno);
    return -1;
  }

  if (setsockopt(_sock, SOL_SOCKET, SO_RCVBUF,
		 &recvBufSize, sizeof(recvBufSize)) == -1) {
    printf("ERROR: Failed to set socket receiver buffer size (errno=%d)\n", errno);
    return -1;
  }

  int reuse = 1;

  if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR,
		 &reuse, sizeof(reuse)) == -1) {
    printf("ERROR: Failed to set socket reuse address option (errno=%d)\n", errno);
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec = 20;
  timeout.tv_usec = 0;

  if (setsockopt(_sock, SOL_SOCKET, SO_RCVTIMEO,
		 &timeout, sizeof(timeout)) == -1) {
    printf("ERROR: Failed to set socket timeout option (errno=%d)\n", errno);
    return -1;
  }

  struct sockaddr_in sockaddrSrc;
  sockaddrSrc.sin_family = AF_INET;
  sockaddrSrc.sin_addr.s_addr = htonl(address);
  sockaddrSrc.sin_port = htons(port);
  
  if (bind(_sock, (struct sockaddr*) &sockaddrSrc, sizeof(sockaddrSrc)) == -1) {
    printf("ERROR: Failed to bind socket (errno=%d)\n", errno);
  }

  struct sockaddr_in sockaddrName;
  socklen_t len = sizeof(sockaddrName);
  if(getsockname(_sock, (struct sockaddr *) &sockaddrName, &len) == 0) {
    unsigned int sockAddress = ntohl(sockaddrName.sin_addr.s_addr);
    unsigned int sockPort = (unsigned int )ntohs(sockaddrName.sin_port);
    char str[100];
    addressToStr(sockAddress, str);
    printf( "Server addr: %s  Port %u  Buffer Size %u\n",
	    str, sockPort, recvBufSize);
  }
  else {
    printf("ERROR: Failed on getsockname() (errno=%d)\n", errno);
    return -1;
  }


  return 0;
}

static int registerMulticast(unsigned int address, char *interfaceStr) {
  unsigned int interface;
  
  /*
  in_addr_t inetAddr = inet_addr(interfaceStr);

  if (inetAddr == -1) {
    printf("ERROR: Failed to get address for interface \"%s\" (errno=%d)\n",
	   interfaceStr, errno);
    return -1;
  }

  interface = ntohl(inetAddr);
  */

  struct in_addr inp;
  if (inet_aton(interfaceStr, &inp) == 0) {
    printf("ERROR: Failed on inet_aton() (errno=%d)\n", errno);
    return -1;
  }

  interface = ntohl(inp.s_addr);


  if (interface != 0) {
    char str[100];
    addressToStr(interface, str);
    printf("Multicast interface IP: %s (interface %s)\n", str, interfaceStr);

    struct ip_mreq ipMreq;
    memset((char*)&ipMreq, 0, sizeof(ipMreq));
    ipMreq.imr_multiaddr.s_addr = htonl(address);
    ipMreq.imr_interface.s_addr = htonl(interface);
    if (setsockopt(_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&ipMreq,
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

static int deleteSocket() {
  close(_sock);
  return 0;
}

static int getMessage(int recvBufSize) {
  size_t recvSize = 0;
  struct msghdr msghdr;
  int flags = 0;
  char buffer[recvSize];

  struct iovec       iov; // Buffer description socket receive
  struct sockaddr_in src; // Socket name source machine

  iov.iov_base = &buffer;
  iov.iov_len  = recvBufSize;               

  memset((void*)&msghdr, 0, sizeof(msghdr));
  msghdr.msg_name       = (caddr_t)&src;
  msghdr.msg_namelen    = sizeof(src);
  msghdr.msg_iov        = &iov;
  msghdr.msg_iovlen     = 1;

  printf("Waiting for BLD package\n");
  recvSize = recvmsg(_sock, &msghdr, flags);

  if (recvSize < 0) {
    printf("ERROR: Failed on recvmsg(...)) (errno=%d)\n", errno);
  }

  if (recvSize == 0) {
    printf("Message size: ZERO (errno=%d)\n", errno);
  }
  else {
    if (recvSize == -1) {
      if (errno == EAGAIN) {
	printf("No messages received, timed out. (errno=%d)\n", errno);
      }
      else {
	printf("ERROR: No messages received (errno=%d)\n", errno);
      }
    }
    else {
      printf("Message size: %d\n", recvSize);
    }
  }

  return recvSize;
}

int getBLD(char *interface) {
  printf("Waiting for BLD packets...\n");

  unsigned int address = ntohl(inet_addr("239.255.24.1"));
  unsigned int port = 10148;
  int recvBufSize = 512;

  if (interface == NULL) {
    printf("Please specify an interface.\n");
    return -1;
  }

  if (createSocket(address, port, recvBufSize) != 0) {
    return -1;
  }

  if (registerMulticast(address, interface) != 0) {
    return -1;
  }

  int size = getMessage(recvBufSize);
  if (size < 0) {
    return -1;
  }

  deleteSocket();

  printf("done.\n");
  return 0;
}
