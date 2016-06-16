/* 

multicast.c

The following program sends or receives multicast packets. If invoked
with one argument, it sends a packet containing the current time to an
arbitrarily chosen multicast group and UDP port. If invoked with no
arguments, it receives and prints these packets. Start it as a sender on
just one host and as a receiver on all the other hosts

*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#if 0
#include "endian.h"
#endif

#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsTypes.h>
#include <initHooks.h>
#include <errlog.h>

#include "BLDTypes.h"
#include "evrTime.h"
#include "evrPattern.h"

#define POSIX_TIME_AT_EPICS_EPOCH 631152000u

#define MCAST_TTL	8
#define DEF_PORT	10148
#define	N_BPMS		36
#define DEF_GROUP	"239.255.24.63"
#define EORBITS_ID	63
#define LOGICAL_ID	0x06000000

int			EORBITS_DEBUG = 1;
epicsExportAddress(int, EORBITS_DEBUG);

uint32_t	Vers_EOrbits	= 0;
uint32_t	Id_EOrbits		= 103;
/* Size includes one XTC header (5 uint32) plus nBPMS plus orbits */
uint32_t	Size_EOrbits	= 20 + 4 + N_BPMS * 3 * sizeof(double);

typedef struct EORBITS
{
	Uint32_LE	ts_nsec;
	Uint32_LE	ts_sec;
	Uint32_LE	uMBZ1;
	Uint32_LE	uFiducialId;
	Uint32_LE	uMBZ2;

	/* Xtc Section 1 */
	uint32_t	uDamage;
	uint32_t	uLogicalId;		/* source 1 */
	uint32_t	uPhysicalId;	/* source 2 */
	uint32_t	uDataType;		/* Contains - this is the version field */
	uint32_t	uExtentSize;	/* Extent - size of data following Xtc Section 2*/

	/* Xtc Section 2 */
	uint32_t	uDamage2;
	uint32_t	uLogicalId2;
	uint32_t	uPhysicalId2;
	uint32_t	uDataType2;
	uint32_t	uExtentSize2;	/* Extent - size of data following Xtc Section 2*/

	/* Data */
	uint32_t	nBPMS;

	Flt64_LE	fBPM_X[N_BPMS];
	Flt64_LE	fBPM_Y[N_BPMS];
	Flt64_LE	fBPM_T[N_BPMS];
}	EORBITS;

static epicsEventId eOrbitsSyncEvent = NULL;

void EVRCallback( void * unused )
{
	/* evrRWMutex is locked while calling these user functions so don't do anything that might block. */
	epicsTimeStamp time_s;

	/* get the current pattern data - check for good status */
	evrModifier_ta modifier_a;
	epicsUInt32  patternStatus; /* see evrPattern.h for values */
	int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
	if ( status != 0 )
	{
		/* Error from evrTimeGetFromPipeline! */
#if 0
		bldFiducialTime.nsec = PULSEID_INVALID;
#endif
		return;
	}

	/* check for LCLS beam */
	if ( (modifier_a[4] & MOD5_BEAMFULL_MASK) == 0 )
	{
		/* No beam */
		return;
	}

#if 0
	/* Get timestamps for beam fiducial */
	bldFiducialTime = time_s;
	bldFiducialTsc	= GetHiResTicks();
#endif

	/* Signal the eOrbitsSyncEvent to trigger the fcomGetBlobSet call */
	epicsEventSignal( eOrbitsSyncEvent );

	return;
}

int	eOrbitsExit	= 0;

int eOrbitsLoop( const char * group, int port, const char * interface )
{
	int					status;
	unsigned int		i;
	struct sockaddr_in	addr;
	int					addrlen, sock, cnt;
	int					mcastTTL = MCAST_TTL;
#if 0
	struct timespec		sendDelay;
#endif
	double				radians	= 0.0;
	uint32_t			ts_fid;
	epicsTimeStamp		ts;
	EORBITS				eOrbits;

	if ( EORBITS_DEBUG >= 1 )
		printf( "Starting eOrbits loop ...\n" );

	/* Initialize header */
	eOrbits.ts_nsec			= __le32( 0 );
	eOrbits.ts_sec			= __le32( 0 );
	eOrbits.uMBZ1			= __le32( 0 );
	eOrbits.uFiducialId		= __le32( 0 );
	eOrbits.uMBZ2			= __le32( 0 );

	/* Xtc Section 1 */
	eOrbits.uDamage			= __le32( 0 );
	eOrbits.uLogicalId		= __le32( LOGICAL_ID );
	eOrbits.uPhysicalId		= __le32( EORBITS_ID );
	eOrbits.uDataType		= __le32( (Vers_EOrbits << 16) + Id_EOrbits );	
	eOrbits.uExtentSize		= __le32( Size_EOrbits );

	/* Xtc Section 2 */
	eOrbits.uDamage2		= __le32( 0 );
	eOrbits.uLogicalId2		= __le32( LOGICAL_ID );
	eOrbits.uPhysicalId2	= __le32( EORBITS_ID );
	eOrbits.uDataType2		= __le32( (Vers_EOrbits << 16) + Id_EOrbits );	
	eOrbits.uExtentSize2	= __le32( Size_EOrbits );

	/* Data */
	eOrbits.nBPMS			= __le32( N_BPMS );

	for ( i = 0; i < N_BPMS; i++ )
	{
		__st_le64( &eOrbits.fBPM_X[i], 0.0 );
		__st_le64( &eOrbits.fBPM_Y[i], 0.0 );
		__st_le64( &eOrbits.fBPM_T[i], 0.0 );
	}

	/* set up socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("Socket creation failed!");
		exit(1);
	}
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family			= AF_INET;
	addr.sin_addr.s_addr	= inet_addr(group);
	addr.sin_port			= htons(port);
	addrlen	= sizeof(addr);

	if ( setsockopt( sock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&mcastTTL, sizeof(unsigned char) ) < 0 )
	{
		perror("setsockopt IP_MULTICAST_TTL failed!");
		exit(1);
	}
	if ( interface )
	{
		struct in_addr		address;
		static in_addr_t	mcastIntfIp;

		mcastIntfIp = inet_addr( interface );
		if ( mcastIntfIp == (in_addr_t)(-1) )
		{
			printf( "Invalid interface IP: %s\n", interface );
			exit(1);
		}

		address.s_addr	= mcastIntfIp;
		if ( setsockopt( sock, IPPROTO_IP, IP_MULTICAST_IF, (char*)&address, sizeof(struct in_addr) ) < 0 )
		{
			perror("setsockopt IP_MULTICAST_IF failed!");
			exit(1);
		}
	}

	if ( interface )
		printf("Sending: eOrbit BLD to %s port %d via %s\n", group, port, interface );
	else
		printf("Sending: eOrbit BLD to %s port %d\n", group, port );

#if 0
	ts_sec	= time(0) + POSIX_TIME_AT_EPICS_EPOCH;
	ts_sec	= 0;
	ts_nsec	= 0;
	ts_fid	= 0;
#endif

	/* All ready to go, create event and register with EVR */
	eOrbitsSyncEvent = epicsEventMustCreate(epicsEventEmpty);

	/* Register EVRCallback */
	evrTimeRegister(EVRCallback, NULL);

	while (1)
	{
		double		sinVal	= sin(radians);
		double		cosVal	= cos(radians);
		radians += 2 * M_PI / 360;
   
   		if ( eOrbitsExit )
			break;
#if 0
		/* set up nanosleep delay for approx 120hz */
		sendDelay.tv_sec	= 0;
		sendDelay.tv_nsec	= 8333333;
		nanosleep( &sendDelay, NULL );
		status = epicsEventWaitOK;
#else
		status = epicsEventWaitWithTimeout( eOrbitsSyncEvent, 0.5 );
#endif
		if(status != epicsEventWaitOK)
		{
			if(status == epicsEventWaitTimeout)
			{
				if(EORBITS_DEBUG >= 2) errlogPrintf("Timed out waiting for Beam event\n");
				continue;
			}
			else
			{
				errlogPrintf("Wait EVR Error, what happened? Let's sleep 2 seconds.\n");
				epicsThreadSleep(2.0);
			}
			continue;
		}

		epicsTimeGetEvent( &ts, 40 );
		ts_fid = ts.nsec & 0x1FFFF;

		/* Update eOrbits w/ simulated data */
		eOrbits.ts_sec		= __le32( ts.secPastEpoch );
		eOrbits.ts_nsec		= __le32( ts.nsec );
		eOrbits.uFiducialId	= __le32( ts_fid );
		for ( i = 0; i < N_BPMS; i++ )
		{
			__st_le64( &eOrbits.fBPM_X[i], sinVal );
			__st_le64( &eOrbits.fBPM_Y[i], cosVal );
			__st_le64( &eOrbits.fBPM_T[i], sinVal );
		}

		if ( EORBITS_DEBUG >= 3 )
		{
			uint32_t	fid = __ld_le32( &eOrbits.ts_nsec ) & 0x1FFFF;
			double		sec = __ld_le32( &eOrbits.ts_sec );
			sec += __ld_le32( &eOrbits.ts_nsec ) / 1e9;
			if ( interface )
				printf("Sending: eOrbit BLD to %s port %d via %s, ts %.3f fid %d\n", group, port, interface, sec, fid );
			else
				printf("Sending: eOrbit BLD to %s port %d, ts %.3f fid %d\n", group, port, sec, fid );
		}
		cnt = sendto(	sock, &eOrbits, sizeof(eOrbits), 0,
						(struct sockaddr *) &addr, addrlen );
		if (cnt < 0)
		{
			perror("sendto");
			exit(1);
		}

#if 0
		ts_nsec	+= sendDelay.tv_nsec;
		if ( ts_nsec > 1000000000 )
		{
			ts_sec++;
			ts_nsec -= 1000000000;
		}
		ts_fid	+= 3;
		if( ts_fid >  0x1FFE0 )
			ts_fid -= 0x1FFE0;
#endif
	}

	return 0;
}

static void eOrbitsTaskEnd(void * parg)
{
    eOrbitsExit = 1;
    epicsThreadSleep(2.0);

    printf("eOrbitsTaskEnd\n");
    return;
}

static int eOrbitsTask(void * parg)
{
	return ( eOrbitsLoop( DEF_GROUP, DEF_PORT, getenv("IPADDR1") ) );
}

void  eOrbitsStart( initHookState state )
{
	if ( state != initHookAfterIocRunning )
		return;
    epicsAtExit(eOrbitsTaskEnd, NULL);
	epicsThreadMustCreate("eOrbits", epicsThreadPriorityMax, 20480, (EPICSTHREADFUNC)eOrbitsTask, NULL);
}

#ifdef	TESTING
int main( int argc, char * argv[] )
{
	const char *	group		= DEF_GROUP;
	int				port		= DEF_PORT;
	const char *	interface	= NULL;
	if (argc > 1)
		group = argv[1];
	if (argc > 2)
		port = atoi(argv[2]);
	if (argc > 3)
		interface = argv[3];

	if (	strcmp( group, "-h"		) == 0
		||	strcmp( group, "--help"	) == 0 )
	{
		printf( "Usage:\n" );
		printf( "   mcastSend GROUP\n" );
		printf( "   mcastSend GROUP PORT\n" );
		printf( "   mcastSend GROUP PORT INTF\n" );
		printf( "   mcastSend --help\n" );
		printf( "Example:\n" );
		printf( "   mcastSend 239.255.24.254 10148 172.27.3.78\n" );
		exit( 0 );
	}

	if ( eOrbitsLoop( group, port, interface ) < 0 )
		exit(1);
	return 0;
}
#endif	/* TESTING */
