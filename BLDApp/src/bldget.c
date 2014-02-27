/* $Id: bldget.c,v 1.4.2.1 2012/03/19 22:32:24 lpiccoli Exp $ */

/* Test Program for dumping the contents of the BLD packet */
#include <stdio.h>
#include <string.h>
#include <udpComm.h>
#include <BLDMCast.h>
#include <osiSock.h>
#include <evrTime.h>
#include <epicsTime.h>
#include <inttypes.h>


static int
mc_setup(const char *mcaddr, unsigned port, const char *mcifaddr)
{
int            sd, err;
struct in_addr ina;

	if ( mcifaddr ) {
		if ( hostToIPAddr(mcifaddr, &ina) ) {
			fprintf(stderr,"Unable to determine address of IF you want to use\n");
			return -1;
		}
		udpCommSetIfMcastInp( ina.s_addr );
	}

	if ( !mcaddr || hostToIPAddr(mcaddr, &ina) ) {
		fprintf(stderr,"Unable to determine address of MC group to join\n");
		return -1;
	}

	if ( (sd = udpCommSocket(port)) < 0 ) {
		fprintf(stderr,"Unable to create socket: %s\n", strerror(-sd));
	} else {
		if ( (err = udpCommJoinMcast(sd, ina.s_addr)) ) {
			fprintf(stderr,"Unable to join MC group: %s\n", strerror(-err));
			udpCommClose(sd);
			sd = err;
		}
	}
	return sd;
}

static void
mc_shutdown(int sd)
{
	if ( sd >= 0 ) {
		udpCommClose(sd);
	}
}

static const char *lbls[] = {
	"  Charge:   ", "[nC]",
	"  L3Energy: ", "[MeV]",
	"  LTUPosX:  ", "[mm]",
	"  LTUPosY:  ", "[mm]",
	"  LTUAngX:  ", "[mrad]",
	"  LTUAngY:  ", "[mrad]",
	"  BunchLen: ", "[A]",
	"  BC1Change:", "[A]",
	"  BC1Energy:", "[MeV]",
};

static void
mc_dump(int sd, int timeout_ms)
{
UdpCommPkt         p;
uint32_t           peer_ip;
uint16_t           peer_pt;
char               buf[150];
epicsTimeStamp     t;
epicsUInt32        mask;
epicsUInt32        pulseid;
uint32_t           utmp;
Flt64_LE           *p_f;
int                i;

struct sockaddr_in sin;

	p = udpCommRecvFrom(sd, timeout_ms, &peer_ip, &peer_pt);
	if ( p ) {
		EBEAMINFO *info = udpCommBufPtr(p);

		sin.sin_family      = AF_INET;
		sin.sin_addr.s_addr = peer_ip;
		sin.sin_port        = htons(peer_pt);

		ipAddrToDottedIP(&sin, buf, sizeof(buf));
		buf[sizeof(buf)-1]=0;

		printf("Received BLD Packet from %s\n", buf);

		t.nsec         = -1;
		mask           = PULSEID(t);

		t.secPastEpoch = __ld_le32(&info->ts_sec);
		t.nsec         = __ld_le32(&info->ts_nsec);

		pulseid        = PULSEID(t);
		t.nsec        &= ~mask;

		epicsTimeToStrftime(buf, sizeof(buf), "%F %T.%06f",&t);
		printf("  Timestamp: %s\n",buf);
		printf("  Pulse ID:  %u",  pulseid);
		if ( pulseid > PULSEID_MAX )
			printf(" (INVALID)");
		printf("\n");

		if ( pulseid != (utmp = __ld_le32(&info->uFiducialId)) )
			printf("ERROR: 'fiducialId doesn't match ?? (now %"PRIu32")\n", utmp);

		if ( 0 != (utmp = __ld_le32(&info->uMBZ1)) ) 
			printf("ERROR: Mandatory-zero field 1 not zero: %"PRIu32" (0x%08"PRIx32")\n", utmp, utmp);
		
		if ( 0 != (utmp = __ld_le32(&info->uMBZ2)) ) 
			printf("ERROR: Mandatory-zero field 2 not zero: %"PRIu32" (0x%08"PRIx32")\n", utmp, utmp);


		printf("  Xtc Section 1\n"); 
		utmp = __ld_le32(&info->uDamage);
		printf("    Damage:      0x%08"PRIx32"\n", utmp);

		utmp = __ld_le32(&info->uLogicalId);

		printf("    Logical  Id: 0x%08"PRIx32, utmp);
		if ( 0x06000000 != utmp )
			printf(" (ERROR: should be 0x06000000)");
		printf("\n");

		utmp = __ld_le32(&info->uPhysicalId);
		printf("    Physical Id: 0x%08"PRIx32, utmp);
		if ( 0x00000000 != utmp )
			printf(" (ERROR: should be 0x00000000)");
		printf("\n");

		utmp = __ld_le32(&info->uDataType);
		printf("    Data Type:   0x%08"PRIx32, utmp);
		if ( 0x0001000f != utmp )
			printf(" (ERROR: should be 0x0001000f)");
		printf("\n");

		utmp = __ld_le32(&info->uExtentSize);
		printf("    Extent Size: 0x%08"PRIx32"\n", utmp);
		if ( 80 != utmp )
			printf(" (ERROR: should be 0x%08x)",80);
		printf("\n");

		printf("  Xtc Section 2\n"); 
		utmp = __ld_le32(&info->uDamage2);
		printf("    Damage:      0x%08"PRIx32"\n", utmp);

		utmp = __ld_le32(&info->uLogicalId2);

		printf("    Logical  Id: 0x%08"PRIx32, utmp);
		if ( 0x06000000 != utmp )
			printf(" (ERROR: should be 0x06000000)");
		printf("\n");

		utmp = __ld_le32(&info->uPhysicalId2);
		printf("    Physical Id: 0x%08"PRIx32, utmp);
		if ( 0x00000000 != utmp )
			printf(" (ERROR: should be 0x00000000)");
		printf("\n");

		utmp = __ld_le32(&info->uDataType2);
		printf("    Data Type:   0x%08"PRIx32, utmp);
		if ( 0x0001000f != utmp )
			printf(" (ERROR: should be 0x0001000f)");
		printf("\n");

		utmp = __ld_le32(&info->uExtentSize2);
		printf("    Extent Size: 0x%08"PRIx32"\n", utmp);
		if ( 80 != utmp )
			printf(" (ERROR: should be 0x%08x)",80);
		printf("\n");

		utmp = __ld_le32(&info->uDamageMask);
		printf("  Damage Mask:   0x%08"PRIx32"\n", utmp);

		for ( p_f = &info->ebeamCharge, mask=1, i=0; p_f <= &info->ebeamBC1Energy; p_f++, mask<<=1, i++ ) {
			printf("%s",lbls[2*i]);
			if ( (utmp & mask) ) {
				printf("      (INVALID)");
			} else {
				printf("%15.6e %s", __ld_le64(p_f), lbls[2*i+1]);
			}
			printf("\n");
		}

		udpCommFreePacket(p);
	} else {
		fprintf(stderr,"Nothing received / timeout\n");
	}
}

void
bldget(const char *mcaddr, unsigned port, const char *mcifaddr, int timeout_ms)
{
int        sd;

	if ( !mcaddr )
		mcaddr = "239.255.24.0";

	if ( 0 == port )
		port   = 10148;

	if ( (sd = mc_setup(mcaddr, port, mcifaddr)) < 0 ) {
		goto bail;
	}

	mc_dump(sd, timeout_ms);

bail:
	mc_shutdown(sd);
}

#ifndef __rtems__
#include <getopt.h>

static void
usage(char *nm)
{
	fprintf(stderr,"usage: %s [-h] [-m multicast_group] [-p port_no] [-t timeout_ms] [-i local_if_addr_for_mcast]\n", nm);
}

int
main(int argc, char **argv)
{
int         ch;
int         rval       = 0;
const char *mcaddr     = 0;
const char *mcifaddr   = 0;
int         timeout_ms = 1000;
unsigned    port       = 0;

	while ( (ch = getopt(argc, argv, "hm:i:t:p:")) > 0 ) {
		switch (ch) {
			default:  rval=1;
			case 'h':
				usage(argv[0]);
				return rval;

			case 'm':
				mcaddr = optarg;
			break;

			case 'i':
				mcifaddr = optarg;
			break;

			case 't':
				if ( 1 != sscanf(optarg, "%i",&timeout_ms) ) {
					fprintf(stderr,"Error: non-numeric timeout value?\n");
					return 1;
				}
			break;

			case 'p':
				if ( 1 != sscanf(optarg, "%u",&port) ) {
					fprintf(stderr,"Error: non-numeric port #?\n");
					return 1;
				}
			break;
		}
	}

	bldget(mcaddr, port, mcifaddr, timeout_ms);
	
	return rval;
}
#endif
