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
#include <epicsMath.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsTypes.h>
#include <initHooks.h>
#include <errlog.h>

#include "BLDTypes.h"
#include "evrTime.h"
#include "evrPattern.h"
#include "udpComm.h"
#include "fcom_api.h"
#include "fcomUtil.h"

#define POSIX_TIME_AT_EPICS_EPOCH 631152000u

#define MCAST_TTL	8
#define DEF_PORT	10148
#define	N_BPMS		36
#define DEF_GROUP	"239.255.24.63"
#define EORBITS_ID	63
#define LOGICAL_ID	0x06000000

int			EORBITS_ENABLE = 0;
epicsExportAddress(int, EORBITS_ENABLE);

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

const char		*	blobNames[N_BPMS] =
{
	"BPMS:LTU1:910:X",
	"BPMS:LTU1:960:X",
	"BPMS:UND1:100:X",
	"BPMS:UND1:190:X",
	"BPMS:UND1:290:X",
	"BPMS:UND1:390:X",
	"BPMS:UND1:490:X",
	"BPMS:UND1:590:X",
	"BPMS:UND1:690:X",
	"BPMS:UND1:790:X",
	"BPMS:UND1:890:X",
	"BPMS:UND1:990:X",
	"BPMS:UND1:1090:X",
	"BPMS:UND1:1190:X",
	"BPMS:UND1:1290:X",
	"BPMS:UND1:1390:X",
	"BPMS:UND1:1490:X",
	"BPMS:UND1:1590:X",
	"BPMS:UND1:1690:X",
	"BPMS:UND1:1790:X",
	"BPMS:UND1:1890:X",
	"BPMS:UND1:1990:X",
	"BPMS:UND1:2090:X",
	"BPMS:UND1:2190:X",
	"BPMS:UND1:2290:X",
	"BPMS:UND1:2390:X",
	"BPMS:UND1:2490:X",
	"BPMS:UND1:2590:X",
	"BPMS:UND1:2690:X",
	"BPMS:UND1:2790:X",
	"BPMS:UND1:2890:X",
	"BPMS:UND1:2990:X",
	"BPMS:UND1:3090:X",
	"BPMS:UND1:3190:X",
	"BPMS:UND1:3290:X",
	"BPMS:UND1:3390:X",
};

static epicsEventId eOrbitsSyncEvent = NULL;

void EVRCallback( void * unused )
{
	/* evrRWMutex is locked while calling these user functions so don't do anything that might block. */
	epicsTimeStamp	time_s;
	int				fid;

	/* get the current pattern data - check for good status */
	evrModifier_ta modifier_a;
	epicsUInt32  patternStatus; /* see evrPattern.h for values */
	int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
	if ( status != 0 )
	{
		/* Error from evrTimeGetFromPipeline! */
		if ( EORBITS_DEBUG >= 3 )
			printf( "EVRCallback: Error %d calling evrTimeGetFromPipeline!\n", status );
#if 0
		bldFiducialTime.nsec = PULSEID_INVALID;
#endif
		return;
	}

	/* check for LCLS beam */
	fid	= time_s.nsec & 0x1FFFF;
	if ( (modifier_a[4] & MOD5_BEAMFULL_MASK) == 0 )
	{
		/* No beam */
		if ( EORBITS_DEBUG >= 6 )
			printf( "EVRCallback: No beam on fid %d.!\n", fid );
		return;
	}

#if 0
	/* Get timestamps for beam fiducial */
	bldFiducialTime = time_s;
	bldFiducialTsc	= GetHiResTicks();
#endif

	if ( EORBITS_DEBUG >= 5 )
		printf( "Signaling eOrbitsSyncEvent: fid %d\n", fid );

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
	int					sFd, cnt;
	int					mcastTTL = MCAST_TTL;
#if 0
	struct timespec		sendDelay;
#endif
	double				radians	= 0.0;
	uint32_t			ts_fid;
	epicsTimeStamp		ts;
	EORBITS				eOrbits;
	FcomID				blobIds[N_BPMS];

	if ( EORBITS_DEBUG >= 1 )
		printf( "Starting eOrbits loop ...\n" );

	/* Translate the blob names to id's */
	for ( i = 0; i < N_BPMS; i++ )
		blobIds[i] = fcomLCLSPV2FcomID( blobNames[i] );

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
#if 1
	sFd = udpCommSocket(0);
	if (sFd < 0)
	{
		perror("eOrbits BLD Socket creation failed!");
		return -1;
	}

	if ( (status = udpCommConnect( sFd, inet_addr(group), port )) ) {
		errlogPrintf("Failed to 'connect' to peer: %s\n", strerror(-status));
		udpCommClose( sFd );
		return -1;
	}

	mcastTTL = MCAST_TTL;
	if ( ( status = udpCommSetMcastTTL( sFd, mcastTTL ) ) ) {
		errlogPrintf("Failed to set multicast TTL: %s\n", strerror(-status));
		udpCommClose( sFd );
		return -1;
	}

	if ( interface )
	{
		in_addr_t	mcastIntfIp = inet_addr(interface);
		if ( mcastIntfIp < 0 || ( status = udpCommSetIfMcastOut( sFd, mcastIntfIp ) ) != 0 )
		{
			errlogPrintf("Failed to choose outgoing interface for multicast: %s\n", strerror(-status));
			udpCommClose( sFd );
			return -1;
		}
	}
#else
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	memset((char *)&addr, 0, sizeof(addr));
	addr.sin_family			= AF_INET;
	addr.sin_addr.s_addr	= inet_addr(group);
	addr.sin_port			= htons(port);
	if ( setsockopt( sock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&mcastTTL, sizeof(unsigned char) ) < 0 )
	{
		perror("setsockopt IP_MULTICAST_TTL failed!\n");
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
#endif

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
	status = evrTimeRegister(EVRCallback, NULL);
	if ( status != 0 )
	{
		errlogPrintf( "eOrbitsLoop: Unable to register EVRCallback!\n");
	}

	while (1)
	{
		double		xVal	= epicsNAN;
		double		yVal	= epicsNAN;
		double		tVal	= epicsNAN;
		radians += 2 * M_PI / 360;
   
   		if ( eOrbitsExit )
			break;
		status = epicsEventWaitWithTimeout( eOrbitsSyncEvent, 10 );
		if(status != epicsEventWaitOK)
		{
			if(status == epicsEventWaitTimeout)
			{
				if(EORBITS_DEBUG >= 2) errlogPrintf("Timed out waiting for eOrbits sync\n");
				continue;
			}
			else
			{
				errlogPrintf("Wait EVR Error, what happened? Let's sleep 2 seconds.\n");
				epicsThreadSleep(2.0);
			}
			continue;
		}

		/* Update eOrbits data */
		ts.secPastEpoch	= 0;
		ts.nsec			= 0x1FFFF;
		ts_fid			= 0x1FFFF;
		for ( i = 0; i < N_BPMS; i++ )
		{
			FcomBlob	*	pBlob;
			int				status;
			status	= fcomGetBlob( blobIds[i], &pBlob, 0 );
			xVal	= epicsNAN;
			yVal	= epicsNAN;
			tVal	= epicsNAN;
			if (	status == 0
				&&	pBlob != NULL
				&&	pBlob->fc_stat == 0
				&&	pBlob->fc_nelm >= 3 )
			{
				if ( pBlob->fc_type == FCOM_EL_DOUBLE )
				{
					xVal = pBlob->fc_dbl[0];
					yVal = pBlob->fc_dbl[1];
					tVal = pBlob->fc_dbl[2];
				}
				else if ( pBlob->fc_type == FCOM_EL_FLOAT )
				{
					xVal = pBlob->fc_flt[0];
					yVal = pBlob->fc_flt[1];
					tVal = pBlob->fc_flt[2];
				}

				if ( ts_fid == 0x1FFFF )
				{	
					/* Grab the timestamp for the first valid blob */
					ts.secPastEpoch		= pBlob->fc_tsHi;
					ts.nsec				= pBlob->fc_tsLo;
					ts_fid				= ts.nsec & 0x1FFFF;
					eOrbits.ts_sec		= __le32( ts.secPastEpoch );
					eOrbits.ts_nsec		= __le32( ts.nsec );
					eOrbits.uFiducialId	= __le32( ts_fid );
				}
				else
				{
					/* Compare the timestamps */
					if (	ts.secPastEpoch != pBlob->fc_tsHi
						||	ts_fid			!= (pBlob->fc_tsLo & 0x1FFFF ) )
					{
						if ( EORBITS_DEBUG >= 2 )
							errlogPrintf(	"Blob 0x%X, %s, fid 0x%X tsMismatch vs prior 0x%X\n",
											(unsigned int) blobIds[i], blobNames[i],
											(unsigned int) (pBlob->fc_tsLo & 0x1FFFF ),
											(unsigned int) ts_fid );
					}
				}
			}
			if ( pBlob != NULL )
				fcomReleaseBlob( &pBlob );

			__st_le64( &eOrbits.fBPM_X[i], xVal );
			__st_le64( &eOrbits.fBPM_Y[i], yVal );
			__st_le64( &eOrbits.fBPM_T[i], tVal );
		}

		if ( EORBITS_ENABLE == 0 )
			continue;

		/* Fallback timestamp */
		if ( ts.secPastEpoch == 0 || ts.nsec == 0x1FFFF )
		{
			epicsTimeGetEvent( &ts, 40 );
			ts_fid = ts.nsec & 0x1FFFF;
			eOrbits.ts_sec		= __le32( ts.secPastEpoch );
			eOrbits.ts_nsec		= __le32( ts.nsec );
			eOrbits.uFiducialId	= __le32( ts_fid );
		}

		if ( EORBITS_DEBUG >= 3 )
		{
			unsigned int	fid = __ld_le32( &eOrbits.ts_nsec ) & 0x1FFFF;
			double			sec = __ld_le32( &eOrbits.ts_sec );
			sec += __ld_le32( &eOrbits.ts_nsec ) / 1e9;
			if ( interface )
				printf("Sending: eOrbit BLD to %s port %d via %s, ts %.3f fid %u\n", group, port, interface, sec, fid );
			else
				printf("Sending: eOrbit BLD to %s port %d, ts %.3f fid %u\n", group, port, sec, fid );
		}
#if 1
		cnt = udpCommSend( sFd, &eOrbits, sizeof(eOrbits) );
#else
		{
		int					addrlen	= sizeof(addr);
		cnt = sendto(	sFd, &eOrbits, sizeof(eOrbits), 0,
						(struct sockaddr *) &addr, addrlen );
		}
#endif
		if (cnt < 0)
		{
			perror( "eOrbitsLoop: BLD send error\n" );
			continue;
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
	FcomID			blobId;
	unsigned int	i;

	/* Subscribe to the blobs */
	for ( i = 0; i < N_BPMS; i++ )
	{
		blobId = fcomLCLSPV2FcomID( blobNames[i] );
		if ( blobId != FCOM_ID_NONE )
			fcomSubscribe( blobId, FCOM_ASYNC_GET );
		else
			printf( "Error: Invalid blob name %s\n", blobNames[i] );
	}
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
