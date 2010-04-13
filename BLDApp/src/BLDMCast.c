/* $Id: BLDMCast.c,v 1.37 2010/04/08 22:00:16 strauman Exp $ */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>

#include "epicsExport.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "cadef.h"
#include "dbScan.h"
#include "drvSup.h"
#include "alarm.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExit.h"

#include "evrTime.h"
#include "evrPattern.h"

#include "fcom_api.h"
#include "fcomUtil.h"
#include "udpComm.h"
#include "fcomLclsBpm.h"
#include "fcomLclsBlen.h"

#include <devBusMapped.h>

#include <bsp/gt_timer.h> /* This is MVME6100/5500 only */

#include "BLDMCast.h"

#define BLD_DRV_VERSION "BLD driver $Revision: 1.37 $/$Name:  $"

#define CA_PRIORITY     CA_PRIORITY_MAX         /* Highest CA priority */

#define TASK_PRIORITY   epicsThreadPriorityMax  /* Highest EPICS thread priority */

#define DEFAULT_CA_TIMEOUT	0.04		/* Default CA timeout, for 30Hz */
#define DEFAULT_EVR_TIMEOUT	0.2		/* Default EVR event timeout, for 30Hz */

/* We monitor static PVs and update their value upon modification */
enum STATICPVSINDEX
{
    DSPR1=0,
    DSPR2,
    E0BDES,
    FMTRX
};/* the definition here must match the PV definition below */

/* We use caget to read pulse by pulse PVs when EVR triggers */
enum PULSEPVSINDEX
{
    BMCHARGE=0,
    BMENERGY1X,
    BMENERGY2X,
    BMPOSITION1X,
    BMPOSITION2X,
    BMPOSITION3X,
    BMPOSITION4X,
    BMBUNCHLEN,
	/* Define Y after everything else so that fcom array doesn't have to use them */
    BMPOSITION1Y,
    BMPOSITION2Y,
    BMPOSITION3Y,
    BMPOSITION4Y
};/* the definition here must match the PV definition below, the order is critical as well */

enum PVAVAILMASK
{
    AVAIL_DSPR1         = 1<< 0,
    AVAIL_DSPR2         = 1<< 1,
    AVAIL_E0BDES        = 1<< 2,
    AVAIL_FMTRX         = 1<< 3,
    AVAIL_BMCHARGE      = 1<< 4,
    AVAIL_BMENERGY1X    = 1<< 5,
    AVAIL_BMENERGY2X    = 1<< 6,
    AVAIL_BMPOSITION1X  = 1<< 7,
    AVAIL_BMPOSITION2X  = 1<< 8,
    AVAIL_BMPOSITION3X  = 1<< 9,
    AVAIL_BMPOSITION4X  = 1<<10,
    AVAIL_BMPOSITION1Y  = 1<<11,
    AVAIL_BMPOSITION2Y  = 1<<12,
    AVAIL_BMPOSITION3Y  = 1<<13,
    AVAIL_BMPOSITION4Y  = 1<<14,
    AVAIL_BMBUNCHLEN    = 1<<15,
};

/* Structure representing one PV (= channel) */
typedef struct BLDPV
{
    const char *	         name;
    unsigned long	         nElems; /* type is always DOUBLE */

	enum PVAVAILMASK         availMask;

    chid		             pvChId;

    struct dbr_time_double * pTD;

/* No need for eventId, never cancel subscription *\
    evid  pvEvId;
\* No need for eventId, never cancel subscription */

/* No need to hold type, always double *\
    long  dbfType;
    long  dbrType;
\* No need to hold type, always double */
} BLDPV;

typedef struct BLDBLOB {
	const char *            name;
	FcomID                  fcid;
	FcomBlobRef             blob;
	enum PVAVAILMASK        aMsk;
} BLDBLOB;


BLDPV bldStaticPVs[]=
{
    [DSPR1]  = {"BLD:SYS0:500:DSPR1",  1, AVAIL_DSPR1,  NULL, NULL},	/* For Energy */
    [DSPR2]  = {"BLD:SYS0:500:DSPR2",  1, AVAIL_DSPR2,  NULL, NULL},	/* For Energy */
    [E0BDES] = {"BEND:LTU0:125:BDES",  1, AVAIL_E0BDES, NULL, NULL},	/* Energy in MeV */
    [FMTRX]  = {"BLD:SYS0:500:FMTRX", 32, AVAIL_FMTRX,  NULL, NULL}	/* For Position */
};
#define N_STATIC_PVS (sizeof(bldStaticPVs)/sizeof(bldStaticPVs[0]))

BLDPV bldPulsePVs[]=
{
#if 0
    Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
#endif
    [BMCHARGE] = {"BPMS:IN20:221:TMIT", 1, AVAIL_BMCHARGE   , NULL, NULL},	/* Charge in Nel, 1.602e-10 nC per Nel*/

#if 0
    Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
    where E0 is the final desired energy at the LTU (the magnet setting BEND:LTU0:125:BDES*1000)
    dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
    BPM1x = [BPMS:LTU1:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
    BPM2x = [BPMS:LTU1:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
#endif
    [BMENERGY1X] = {"BPMS:LTU1:250:X", 1, AVAIL_BMENERGY1X  , NULL, NULL},	/* Energy in MeV */
    [BMENERGY2X] = {"BPMS:LTU1:450:X", 1, AVAIL_BMENERGY2X  , NULL, NULL},	/* Energy in MeV */

#if 0
    Position X, Y, Angle X, Y at LTU:
    Using the LTU Feedback BPMs: BPMS:LTU1:720,730,740,750
    The best estimate calculation is a matrix multiply for the result vector p:

        [xpos(mm)             [bpm1x         //= BPMS:LTU1:720:X (mm)
    p=   ypos(mm)      = [F]*  bpm2x         //= BPMS:LTU1:730:X
         xang(mrad)            bpm3x         //= BPMS:LTU1:740:X
         yang(mrad)]           bpm4x         //= BPMS:LTU1:750:X
                               bpm1y         //= BPMS:LTU1:720:Y
                               bpm2y         //= BPMS:LTU1:730:Y
                               bpm3y         //= BPMS:LTU1:740:Y
                               bpm4y]        //= BPMS:LTU1:750:Y

    Where F is the 4x8 precalculated fit matrix.  F is approx. pinv(A), for Least Squares fit p = pinv(A)*x

    A = [R11 R12 R13 R14;     //rmat elements for bpm1x
         R11 R12 R13 R14;     //rmat elements for bpm2x
         R11 R12 R13 R14;     //rmat elements for bpm3x
         R11 R12 R13 R14;     //rmat elements for bpm4x
         R31 R32 R33 R34;     //rmat elements for bpm1y
         R31 R32 R33 R34;     //rmat elements for bpm2y
         R31 R32 R33 R34;     //rmat elements for bpm3y
         R31 R32 R33 R34]     //rmat elements for bpm4y
#endif

    [BMPOSITION1X] = {"BPMS:LTU1:720:X", 1, AVAIL_BMPOSITION1X, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION2X] = {"BPMS:LTU1:730:X", 1, AVAIL_BMPOSITION2X, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION3X] = {"BPMS:LTU1:740:X", 1, AVAIL_BMPOSITION3X, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION4X] = {"BPMS:LTU1:750:X", 1, AVAIL_BMPOSITION4X, NULL, NULL},	/* Position in mm/mrad */
    [BMBUNCHLEN]   = {"BLEN:LI24:886:BIMAX", 1, AVAIL_BMBUNCHLEN, NULL, NULL},	/* Bunch Length in Amps */
    [BMPOSITION1Y] = {"BPMS:LTU1:720:Y", 1, AVAIL_BMPOSITION1Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION2Y] = {"BPMS:LTU1:730:Y", 1, AVAIL_BMPOSITION2Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION3Y] = {"BPMS:LTU1:740:Y", 1, AVAIL_BMPOSITION3Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION4Y] = {"BPMS:LTU1:750:Y", 1, AVAIL_BMPOSITION4Y, NULL, NULL},	/* Position in mm/mrad */

};
#define N_PULSE_PVS (sizeof(bldPulsePVs)/sizeof(bldPulsePVs[0]))

BLDBLOB bldPulseBlobs[] =
{
#if 0
    Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
#endif
    [BMCHARGE] = { name: "BPMS:IN20:221:TMIT", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMCHARGE},	/* Charge in Nel, 1.602e-10 nC per Nel*/

#if 0
    Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
    where E0 is the final desired energy at the LTU (the magnet setting BEND:LTU0:125:BDES*1000)
    dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
    BPM1x = [BPMS:LTU1:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
    BPM2x = [BPMS:LTU1:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
#endif
    [BMENERGY1X] = { name: "BPMS:LTU1:250:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMENERGY1X},	/* Energy in MeV */
    [BMENERGY2X] = { name: "BPMS:LTU1:450:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMENERGY2X},	/* Energy in MeV */

#if 0
    Position X, Y, Angle X, Y at LTU:
    Using the LTU Feedback BPMs: BPMS:LTU1:720,730,740,750
    The best estimate calculation is a matrix multiply for the result vector p:

        [xpos(mm)             [bpm1x         //= BPMS:LTU1:720:X (mm)
    p=   ypos(mm)      = [F]*  bpm2x         //= BPMS:LTU1:730:X
         xang(mrad)            bpm3x         //= BPMS:LTU1:740:X
         yang(mrad)]           bpm4x         //= BPMS:LTU1:750:X
                               bpm1y         //= BPMS:LTU1:720:Y
                               bpm2y         //= BPMS:LTU1:730:Y
                               bpm3y         //= BPMS:LTU1:740:Y
                               bpm4y]        //= BPMS:LTU1:750:Y

    Where F is the 4x8 precalculated fit matrix.  F is approx. pinv(A), for Least Squares fit p = pinv(A)*x

    A = [R11 R12 R13 R14;     //rmat elements for bpm1x
         R11 R12 R13 R14;     //rmat elements for bpm2x
         R11 R12 R13 R14;     //rmat elements for bpm3x
         R11 R12 R13 R14;     //rmat elements for bpm4x
         R31 R32 R33 R34;     //rmat elements for bpm1y
         R31 R32 R33 R34;     //rmat elements for bpm2y
         R31 R32 R33 R34;     //rmat elements for bpm3y
         R31 R32 R33 R34]     //rmat elements for bpm4y
#endif

    [BMPOSITION1X] = { name: "BPMS:LTU1:720:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y },	/* Position in mm/mrad */
    [BMPOSITION2X] = { name: "BPMS:LTU1:730:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y },	/* Position in mm/mrad */
    [BMPOSITION3X] = { name: "BPMS:LTU1:740:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y },	/* Position in mm/mrad */
    [BMPOSITION4X] = { name: "BPMS:LTU1:750:X", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y },	/* Position in mm/mrad */
    [BMBUNCHLEN]   = { name: "BLEN:LI24:886:BIMAX", fcid: FCOM_ID_NONE, blob: 0, aMsk: AVAIL_BMBUNCHLEN },	/* Bunch Length in Amps */
};

#define N_PULSE_BLOBS (sizeof(bldPulseBlobs)/sizeof(bldPulseBlobs[0]))

static enum PVAVAILMASK dataAvailable = 0;

#define MCAST_LOCAL_IP	"172.27.225.21"

#define MCAST_TTL	8

#undef  FETCH_PULSE_PVS     /* Otherwise we monitor pulse PVs */
#undef  USE_CA_ADD_EVENT	/* ca_create_subscription seems buggy */
#define USE_PULSE_CA        /* Use CA for obtaining pulse PVs; use FCOM otherwise */
/* T.S: I doubt that ca_create_subscription works any different. 
 * ca_add_array_event is just a macro mapping to the
 * ca_add_masked_array_event() wrapper which calls
 * ca_create_subscription with the DBE_VALUE | DBE_ALARM mask ...
 */
#define MULTICAST           /* Use multicast interface */
#define MULTICAST_UDPCOMM	/* Use UDPCOMM for data output; BSD sockets otherwise */

#define BSPTIMER    0       /* Timer instance -- use first timer */

int BLD_MCAST_ENABLE = 1;
epicsExportAddress(int, BLD_MCAST_ENABLE);

int BLD_MCAST_DEBUG = 0;
epicsExportAddress(int, BLD_MCAST_DEBUG);

static uint32_t timer_delay_clicks = 10 /* just some value; true thing is written by EPICS record */;

int DELAY_FOR_CA = 60;	/* in seconds */
epicsExportAddress(int, DELAY_FOR_CA);

/* Share with device support */
volatile int bldAllPVsConnected   = FALSE;
epicsUInt32  bldInvalidAlarmCount[N_PULSE_BLOBS] = {0};
epicsUInt32  bldUnmatchedTSCount[N_PULSE_BLOBS]  = {0};
EBEAMINFO    bldEbeamInfo;
IOSCANPVT    bldIoscan;         /* Trigger EPICS record */
epicsMutexId bldMutex = NULL;	/* Protect bldStaticPVs' values */
epicsUInt32  bldFcomGetErrs[N_PULSE_BLOBS]       = {0};
epicsUInt32  bldMcastMsgSent                     =  0;
/* Share with device support */

/*==========================================================*/
static in_addr_t mcastIntfIp = 0;

static epicsEventId EVRFireEvent = NULL;

epicsTimeStamp bldFiducialTime;

int      tsCatch = -1;
uint32_t tsMismatch[4];



static void evr_timer_isr(void *arg);
static int BLDMCastTask(void * parg);
static void BLDMCastTaskEnd(void * parg);

int BLDMCastStart(int enable, const char * NIC)
{/* This function will be called in st.cmd after iocInit() */
    /* Do we need to use RTEMS task priority to get higher priority? */
    BLD_MCAST_ENABLE = enable;
    if(NIC && NIC[0] != 0)
        mcastIntfIp = inet_addr(NIC);
    if(mcastIntfIp == (in_addr_t)(-1))
    {
	errlogPrintf("Illegal MultiCast NIC IP\n");
	return -1;
    }

    scanIoInit(&bldIoscan);

    bldMutex = epicsMutexMustCreate();

    /******************************************************************* Setup high resolution timer ***************************************************************/
    /* you need to setup the timer only once (to connect ISR) */
    BSP_timer_setup( BSPTIMER, evr_timer_isr, 0, 0 /* do not reload timer when it expires */);

    epicsAtExit(BLDMCastTaskEnd, NULL);

    return (int)(epicsThreadMustCreate("BLDMCast", TASK_PRIORITY, 20480, (EPICSTHREADFUNC)BLDMCastTask, NULL));
}

static int
pvTimePulseIdMatches(
	epicsTimeStamp *p_ref,
#ifdef USE_PULSE_CA
epicsTimeStamp *p_cmp
#else
	FcomBlobRef p_cmp
#endif
)
{
epicsUInt32 idref, idcmp, diff;

	idref = PULSEID((*p_ref));
#ifdef USE_PULSE_CA
	idcmp = PULSEID((*p_cmp));
#else
	idcmp = p_cmp->fc_tsLo & LOWER_17_BIT_MASK;
#endif

	if ( idref == idcmp && idref != PULSEID_INVALID ) {
		/* Verify that seconds match to less that a few minutes */
		diff = abs(p_ref->secPastEpoch -
#ifdef USE_PULSE_CA
		           p_cmp->secPastEpoch
#else
		           p_cmp->fc_tsHi
#endif
		       );
		if ( diff < 4*60 )
			return 0;
	}

	return -1;
}

int      modi=6*3;
uint32_t mod2[6*3];
uint32_t mod5[6*3];

uint32_t mint=-1;

void
moddmp(void)
{
int i;
    printf("    MOD2   --     MOD5\n");
	for ( i=0; i<sizeof(mod2)/sizeof(mod2[0]); i++ ) {
		printf("0x%08"PRIx32" -- 0x%08"PRIx32"\n", mod2[i], mod5[i]);
	}
}

void EVRFire(void *unused)
{/* This function will be registered with EVR callback */
	epicsUInt32 rate_mask = MOD5_30HZ_MASK|MOD5_10HZ_MASK|MOD5_5HZ_MASK|MOD5_1HZ_MASK|MOD5_HALFHZ_MASK;  /* can be 30HZ,10HZ,5HZ,1HZ,HALFHZ */
	epicsTimeStamp time_s;

	/* get the current pattern data - check for good status */
	evrModifier_ta modifier_a;
	unsigned long  patternStatus; /* see evrPattern.h for values */
	int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
	/* This is 120Hz. So printf will screw timing */
	if(BLD_MCAST_DEBUG >= 4) errlogPrintf("EVR fires\n");
	if (!status)
	{/* check for LCLS beam and rate-limiting */
		if ((modifier_a[4] & MOD5_BEAMFULL_MASK) && (modifier_a[4] & rate_mask))
		{/* ... do beam-sync rate-limited processing here ... */
			/* call 'BSP_timer_start()' to set/arm the hardware */

			bldFiducialTime = time_s;
			/* This is 30Hz. So printf might screw timing */
			if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Timer Starts\n");
			BSP_timer_start( BSPTIMER, timer_delay_clicks );
			{
				uint32_t tt = BSP_timer_read(BSPTIMER);
				if (tt<mint)
					mint = tt;
			}

		}
		if ( modi < sizeof(mod2)/sizeof(mod2[0]) ) {
			mod2[modi] = modifier_a[1];
			mod5[modi] = modifier_a[4];
			modi++;
		}
	} else {
		bldFiducialTime.nsec = PULSEID_INVALID;
	}

	return;
}

static void evr_timer_isr(void *arg)
{/* post event/release sema to wakeup worker task here */
    if(EVRFireEvent) epicsEventSignal(EVRFireEvent);
    /* This is 30Hz. So printk might screw timing */
    if(BLD_MCAST_DEBUG >= 3) printk("Timer fires\n");
    return;
}

static void printChIdInfo(chid pvChId, char *message)
{
    errlogPrintf("\n%s\n",message);
    errlogPrintf("pv: %s  type(%d) nelements(%ld) host(%s)",
	ca_name(pvChId),ca_field_type(pvChId),ca_element_count(pvChId),ca_host_name(pvChId));
    errlogPrintf(" read(%d) write(%d) state(%d)\n", ca_read_access(pvChId),ca_write_access(pvChId),ca_state(pvChId));
}

static void exceptionCallback(struct exception_handler_args args)
{
    chid	pvChId = args.chid;
    long	stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (pvChId ? ca_name(pvChId) : noname);

    if(pvChId) printChIdInfo(pvChId,"exceptionCallback");
    errlogPrintf("exceptionCallback stat %s channel %s\n", ca_message(stat),channel);
}

#if 0
static void accessRightsCallback(struct access_rights_handler_args args)
{
    chid	pvChId = args.chid;

    if(BLD_MCAST_DEBUG >= 2) printChIdInfo(pvChId,"accessRightsCallback");
}

static void connectionCallback(struct connection_handler_args args)
{
    chid	pvChId = args.chid;

    if(BLD_MCAST_DEBUG >= 2) printChIdInfo(pvChId,"connectionCallback");
}
#endif

static void eventCallback(struct event_handler_args args)
{
    BLDPV * pPV = args.usr;

    if(BLD_MCAST_DEBUG >= 2) errlogPrintf("Event Callback: %s, status %d\n", ca_name(args.chid), args.status);

    epicsMutexLock(bldMutex);
    if (args.status == ECA_NORMAL)
    {
        memcpy(pPV->pTD, args.dbr, dbr_size_n(args.type, args.count));
        if(pPV->pTD->severity >= INVALID_ALARM)	/* We don't care timestamp for bldStaticPVs. As long as it is not invalid, they are ok */
			dataAvailable &= ~ pPV->availMask;
        else
			dataAvailable |=   pPV->availMask;

        if(BLD_MCAST_DEBUG >= 2)
        {
            errlogPrintf("Event Callback: %s, copy type %ld, elements %ld, bytes %d, severity %d\n",
                ca_name(args.chid), args.type, args.count, dbr_size_n(args.type, args.count), pPV->pTD->severity);
            errlogPrintf("Value is %f\n", pPV->pTD->value);
        }
    }
    else
    {
		dataAvailable &= ~ pPV->availMask;
    }
    epicsMutexUnlock(bldMutex);
}

static void BLDMCastTaskEnd(void * parg)
{
    int loop;

    bldAllPVsConnected = FALSE;
    epicsThreadSleep(2.0);

    epicsMutexLock(bldMutex);
    for(loop=0; loop<N_STATIC_PVS; loop++)
    {
        if(bldStaticPVs[loop].pvChId)
        {
            ca_clear_channel(bldStaticPVs[loop].pvChId);
            bldStaticPVs[loop].pvChId = NULL;
        }
        if(bldStaticPVs[loop].pTD)
        {
            free(bldStaticPVs[loop].pTD);
            bldStaticPVs[loop].pTD = NULL;
        }
    }

    for(loop=0; loop<N_PULSE_PVS; loop++)
    {
        if(bldPulsePVs[loop].pvChId)
        {
            ca_clear_channel(bldPulsePVs[loop].pvChId);
            bldPulsePVs[loop].pvChId = NULL;
        }
        if(bldPulsePVs[loop].pTD)
        {
            free(bldPulsePVs[loop].pTD);
            bldPulsePVs[loop].pTD = NULL;
        }
    }
    epicsMutexUnlock(bldMutex);
    printf("BLDMCastTaskEnd\n");

    return;
}

static int BLDMCastTask(void * parg)
{
int		loop;
int		rtncode;

int		sFd;
#ifndef MULTICAST_UDPCOMM
struct sockaddr_in sockaddrSrc;
struct sockaddr_in sockaddrDst;
unsigned char mcastTTL;
#endif
epicsTimeStamp *p_refTime;

	/******** Prepare MultiCast **************************************************/
#ifdef MULTICAST
#ifdef MULTICAST_UDPCOMM
	sFd = udpCommSocket(0);
	if ( sFd < 0 ) {
		errlogPrintf("Failed to create socket for multicast: %s\n", strerror(-sFd));
		epicsMutexDestroy(bldMutex);
		bldMutex = 0;
		return -1;
	}

	if ( (rtncode = udpCommConnect( sFd, inet_addr(BLDMCAST_DST_IP), BLDMCAST_DST_PORT )) ) {
		errlogPrintf("Failed to 'connect' to peer: %s\n", strerror(-rtncode));
		epicsMutexDestroy( bldMutex );
		bldMutex = 0;
		udpCommClose( sFd );
		return -1;
	}

	if ( mcastIntfIp && ( rtncode = udpCommSetIfMcastOut( sFd, mcastIntfIp ) ) ) {
		errlogPrintf("Failed to choose outgoing interface for multicast: %s\n", strerror(-rtncode));
		epicsMutexDestroy( bldMutex );
		bldMutex = 0;
		udpCommClose( sFd );
		return -1;
	}
#else
	sFd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sFd == -1)
	{
		errlogPrintf("Failed to create socket for multicast\n");
		epicsMutexDestroy(bldMutex);
		return -1;
	}

	/* no need to enlarge buffer since we have small packet, system default
	 * should be big enough.
	 */

	memset(&sockaddrSrc, 0, sizeof(struct sockaddr_in));
	memset(&sockaddrDst, 0, sizeof(struct sockaddr_in));

	sockaddrSrc.sin_family      = AF_INET;
	sockaddrSrc.sin_addr.s_addr = INADDR_ANY;
	sockaddrSrc.sin_port        = 0;

	sockaddrDst.sin_family      = AF_INET;
	sockaddrDst.sin_addr.s_addr = inet_addr(BLDMCAST_DST_IP);
	sockaddrDst.sin_port        = htons(BLDMCAST_DST_PORT);

	if( bind( sFd, (struct sockaddr *) &sockaddrSrc, sizeof(struct sockaddr_in) ) == -1 )
	{
		errlogPrintf("Failed to bind local socket for multicast\n");
		close(sFd);
		epicsMutexDestroy(bldMutex);
		return -1;
	}

	if(BLD_MCAST_DEBUG >= 2)
	{
		struct sockaddr_in sockaddrName;
#ifdef __linux__
		unsigned int iLength = sizeof(sockaddrName);
#elif defined(__rtems__)
		socklen_t iLength = sizeof(sockaddrName);
#else
		int iLength = sizeof(sockaddrName);
#endif
		if(getsockname(sFd, (struct sockaddr*)&sockaddrName, &iLength) == 0)
			printf( "Local addr: %s Port %u\n", inet_ntoa(sockaddrName.sin_addr), ntohs(sockaddrName.sin_port));
	}

	mcastTTL = MCAST_TTL;
	if (setsockopt(sFd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&mcastTTL, sizeof(unsigned char) ) < 0 )
	{
		perror("Set TTL failed\n");
		errlogPrintf("Failed to set TTL for multicast\n");
		close(sFd);
		epicsMutexDestroy(bldMutex);
		return -1;
	}

	if (mcastIntfIp)
	{
		struct in_addr address;
		address.s_addr = mcastIntfIp;

		if(setsockopt(sFd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&address, sizeof(struct in_addr) ) < 0)
		{
			errlogPrintf("Failed to set interface for multicast\n");
			close(sFd);
			epicsMutexDestroy(bldMutex);
			return -1;
		}
	}
#endif
#endif

	/************************************************************************* Prepare CA *************************************************************************/
	printf("Delay %d seconds to wait CA ready!\n", DELAY_FOR_CA);
	for(loop=DELAY_FOR_CA;loop>0;loop--)
	{
		printf("\r%d seconds left!\n", loop);
		epicsThreadSleep(1.0);
	}
	printf("\n");

	SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");
	SEVCHK(ca_add_exception_event(exceptionCallback,NULL), "ca_add_exception_event");

	/* TODO, clean up resource when fail */
	/* Monitor static PVs */
	for(loop=0; loop<N_STATIC_PVS; loop++)
	{
		rtncode = ECA_NORMAL;
		/* No need for connectionCallback since we want to wait till connected */
		SEVCHK(ca_create_channel(bldStaticPVs[loop].name, NULL/*connectionCallback*/, NULL/*&(bldStaticPVs[loop])*/, CA_PRIORITY ,&(bldStaticPVs[loop].pvChId)), "ca_create_channel");
		/* Not very necessary */
		/* SEVCHK(ca_replace_access_rights_event(bldStaticPVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event"); */

		/* We could do subscription in connetion callback. But in this case, better to enforce all connection */
		rtncode = ca_pend_io(15.0);
		if (rtncode == ECA_TIMEOUT)
		{
			errlogPrintf("Channel connect timed out: '%s' not found.\n", bldStaticPVs[loop].name);
			return -1;
		}
		if(bldStaticPVs[loop].nElems != ca_element_count(bldStaticPVs[loop].pvChId))
		{
			errlogPrintf("Number of elements [%ld] of '%s' does not match expectation.\n", ca_element_count(bldStaticPVs[loop].pvChId), bldStaticPVs[loop].name);
			return -1;
		}

		if(DBF_DOUBLE != ca_field_type(bldStaticPVs[loop].pvChId))
		{/* Native data type has to be double */
			errlogPrintf("Native data type of '%s' is not double.\n", bldStaticPVs[loop].name);
			return -1;
		}

		/* Everything should be double, even not, do conversion */
		bldStaticPVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, bldStaticPVs[loop].nElems), "callocMustSucceed");
#ifdef USE_CA_ADD_EVENT
		SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, bldStaticPVs[loop].nElems, bldStaticPVs[loop].pvChId, eventCallback, &(bldStaticPVs[loop]), 0.0, 0.0, 0.0, NULL), "ca_add_array_event");
#else
		SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, bldStaticPVs[loop].nElems, bldStaticPVs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(bldStaticPVs[loop]), NULL), "ca_create_subscription");
#endif
	}

	ca_flush_io();
	/* ca_pend_event(2.0); */

#ifdef USE_PULSE_CA
	/* We only fetch pulse PVs */
	for(loop=0; loop<N_PULSE_PVS; loop++)
	{
		rtncode = ECA_NORMAL;
		/* No need for connectionCallback since we want to wait till connected */
		SEVCHK(ca_create_channel(bldPulsePVs[loop].name, NULL/*connectionCallback*/, NULL/*&(bldPulsePVs[loop])*/, CA_PRIORITY ,&(bldPulsePVs[loop].pvChId)), "ca_create_channel");
		/* Not very necessary */
		/* SEVCHK(ca_replace_access_rights_event(bldPulsePVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event"); */

		/* We could do subscription in connetion callback. But in this case, better to enforce all connection */
		rtncode = ca_pend_io(15.0);
		if (rtncode == ECA_TIMEOUT)
		{
			errlogPrintf("Channel connect timed out: '%s' not found.\n", bldPulsePVs[loop].name);
			return -1;
		}
		if(bldPulsePVs[loop].nElems != ca_element_count(bldPulsePVs[loop].pvChId))
		{
			errlogPrintf("Number of elements of '%s' does not match expectation.\n", bldPulsePVs[loop].name);
			return -1;
		}

		if(DBF_DOUBLE != ca_field_type(bldPulsePVs[loop].pvChId))
		{/* native type has to be double */
			errlogPrintf("Native data type of '%s' is not double.\n", bldPulsePVs[loop].name);
			return -1;
		}

		/* Everything should be double, even not, do conversion */
		bldPulsePVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, bldPulsePVs[loop].nElems), "callocMustSucceed");
#ifdef FETCH_PULSE_PVS
		/* We don't subscribe to pulse PVs */
#else
#ifdef USE_CA_ADD_EVENT
		SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, bldPulsePVs[loop].nElems, bldPulsePVs[loop].pvChId, eventCallback, &(bldPulsePVs[loop]), 0.0, 0.0, 0.0, NULL), "ca_add_array_event");
#else
		SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, bldPulsePVs[loop].nElems, bldPulsePVs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(bldPulsePVs[loop]), NULL), "ca_create_subscription");
#endif
#endif
	}
	ca_flush_io();
#else
	for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
		if ( FCOM_ID_NONE == (bldPulseBlobs[loop].fcid = fcomLCLSPV2FcomID(bldPulseBlobs[loop].name)) ) {
			errlogPrintf("FATAL ERROR: Unable to determine FCOM ID for PV %s\n", bldPulseBlobs[loop].name);
			return -1;
		}
		rtncode = fcomSubscribe( bldPulseBlobs[loop].fcid, FCOM_ASYNC_GET );
		if ( 0 != rtncode ) {
			errlogPrintf("FATAL ERROR: Unable to subscribe %s (0x%08"PRIx32") to FCOM: %s\n",
					bldPulseBlobs[loop].name,
					bldPulseBlobs[loop].fcid,
					fcomStrerror(rtncode));
			return -1;
		}
	}
#endif


	/* All ready to go, create event and register with EVR */
	EVRFireEvent = epicsEventMustCreate(epicsEventEmpty);
	/* Register EVRFire */
	evrTimeRegister(EVRFire, 0);

	bldAllPVsConnected = TRUE;
	printf("All PVs are successfully connected!\n");

	/* Prefill EBEAMINFO with constant values */
	bldEbeamInfo.uMBZ1       = __le32(0);
	bldEbeamInfo.uMBZ2       = __le32(0);

	bldEbeamInfo.uLogicalId  = __le32(0x06000000);
	bldEbeamInfo.uPhysicalId = __le32(0);
	bldEbeamInfo.uDataType   = __le32(0x1000f);
	bldEbeamInfo.uExtentSize = __le32(80);

	bldEbeamInfo.uLogicalId2 = __le32(0x06000000);
	bldEbeamInfo.uPhysicalId2= __le32(0);
	bldEbeamInfo.uDataType2  = __le32(0x1000f);
	bldEbeamInfo.uExtentSize2= __le32(80);

	while(bldAllPVsConnected)
	{
		int status;

		status = epicsEventWaitWithTimeout(EVRFireEvent, DEFAULT_EVR_TIMEOUT);
		if(status != epicsEventWaitOK)
		{
			if(status == epicsEventWaitTimeout)
			{
				if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Wait EVR timeout, check timing?\n");
			}
			else
			{
				errlogPrintf("Wait EVR Error, what happened? Let's sleep 2 seconds.\n");
				epicsThreadSleep(2.0);
			}

			continue;
		}

		/* This is 30Hz. So printf might screw timing */
		if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Do work!\n");
#ifdef USE_PULSE_CA
#ifdef FETCH_PULSE_PVS
		/* Timer fires ok, let's then get pulse PVs */
		rtncode = ECA_NORMAL;
		for(loop=0; loop<N_PULSE_PVS; loop++)
		{
			rtncode = ca_array_get(DBR_TIME_DOUBLE, bldPulsePVs[loop].nElems, bldPulsePVs[loop].pvChId, (void *)(bldPulsePVs[loop].pTD));
		}
		rtncode = ca_pend_io(DEFAULT_CA_TIMEOUT);
		if (rtncode != ECA_NORMAL)
		{
			if(BLD_MCAST_DEBUG) errlogPrintf("Something wrong when fetch pulse-by-pulse PVs.\n");
			epicsMutexLock(bldMutex);
			bldEbeamInfo.uDamageMask = __le32(0xffffffff); /* no information available */
			bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			epicsMutexUnlock(bldMutex);
		}
		else
#endif
		{/* Got all PVs, do calculation including checking severity and timestamp */

			/* Assume the first timestamp is right */
			p_refTime = & bldPulsePVs[BMCHARGE].pTD->stamp;

			epicsMutexLock(bldMutex);

			__st_le32(&bldEbeamInfo.ts_sec,      p_refTime->secPastEpoch);
			__st_le32(&bldEbeamInfo.ts_nsec,     p_refTime->nsec);
			__st_le32(&bldEbeamInfo.uFiducialId, PULSEID((*p_refTime)));


			for(loop=0; loop<N_PULSE_PVS; loop++)
			{
				dataAvailable |= bldPulsePVs[loop].availMask;

				if(bldPulsePVs[loop].pTD->severity >= INVALID_ALARM )
				{
					dataAvailable &= ~ bldPulsePVs[loop].availMask;
					bldInvalidAlarmCount[0]++;
					if(BLD_MCAST_DEBUG) errlogPrintf("%s has severity %d\n", bldPulsePVs[loop].name, bldPulsePVs[loop].pTD->severity);
				}

				if( 0 != pvTimePulseIdMatches(p_refTime, &(bldPulsePVs[loop].pTD->stamp)) )
				{
					dataAvailable &= ~ bldPulsePVs[loop].availMask;
					bldUnmatchedTSCount[0]++;
					if(BLD_MCAST_DEBUG) errlogPrintf("%s has unmatched timestamp.nsec [0x%08X,0x%08X]\n",
							bldPulsePVs[loop].name, p_refTime->nsec, bldPulsePVs[loop].pTD->stamp.nsec);
				}
			}

			bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			bldEbeamInfo.uDamageMask = __le32(0);

			/* Calculate beam charge */
			if( (dataAvailable & AVAIL_BMCHARGE) )
			{
				__st_le64(&bldEbeamInfo.ebeamCharge, bldPulsePVs[BMCHARGE].pTD->value * 1.602e-10);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(0x1);
			}

#define AVAIL_L3ENERGY (AVAIL_BMENERGY1X | AVAIL_BMENERGY2X | AVAIL_DSPR1 | AVAIL_DSPR2 | AVAIL_E0BDES)

			/* Calculate beam energy */
			if( AVAIL_L3ENERGY == (AVAIL_L3ENERGY & dataAvailable ) )
			{
				double tempD;
				tempD = bldPulsePVs[BMENERGY1X].pTD->value/(1000.0 * bldStaticPVs[DSPR1].pTD->value);
				tempD += bldPulsePVs[BMENERGY2X].pTD->value/(1000.0 * bldStaticPVs[DSPR2].pTD->value);
				tempD = tempD/2.0 + 1.0;
				tempD *= bldStaticPVs[E0BDES].pTD->value * 1000.0;
				__st_le64(&bldEbeamInfo.ebeamL3Energy, tempD);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(0x2);
			}

#define AVAIL_LTUPOS ( AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y | \
		AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y | \
		AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y | \
		AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y | \
		AVAIL_FMTRX )

			/* Calculate beam position */
			if( AVAIL_LTUPOS == (AVAIL_LTUPOS & dataAvailable) )
			{
				dbr_double_t *pMatrixValue;
				double tempDA[4];
				int i,j;

				pMatrixValue = &(bldStaticPVs[FMTRX].pTD->value);
				for(i=0;i<4;i++)
				{
					tempDA[i] = 0.0;
					for(j=0;j<8;j++)
						tempDA[i] += pMatrixValue[i*8+j] * bldPulsePVs[BMPOSITION1X+j].pTD->value;
				}
				__st_le64(&bldEbeamInfo.ebeamLTUPosX, tempDA[0]);
				__st_le64(&bldEbeamInfo.ebeamLTUPosY, tempDA[1]);
				__st_le64(&bldEbeamInfo.ebeamLTUAngX, tempDA[2]);
				__st_le64(&bldEbeamInfo.ebeamLTUAngY, tempDA[3]);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(0x3C);
			}

			/* Copy bunch length */
			if( (AVAIL_BMBUNCHLEN & dataAvailable) )
			{
				__st_le64(&bldEbeamInfo.ebeamBunchLen, bldPulsePVs[BMBUNCHLEN].pTD->value);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(0x40);
			}

			epicsMutexUnlock(bldMutex);

		}
#else /* USE_PULSE_CA */
		/* Use FCOM to obtaine pulse-by-pulse data */
		{
			FcomBlobRef b1;

			p_refTime = &bldFiducialTime;

			epicsMutexLock(bldMutex);

			bldEbeamInfo.uDamageMask = __le32(0);

			for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {

				dataAvailable |= bldPulseBlobs[loop].aMsk;

				rtncode = fcomGetBlob( bldPulseBlobs[loop].fcid, &bldPulseBlobs[loop].blob, 0 );
				if ( rtncode ) {
					bldFcomGetErrs[loop]++;
					dataAvailable        &= ~bldPulseBlobs[loop].aMsk;
					bldPulseBlobs[loop].blob = 0;
					continue;
				}
				b1 = bldPulseBlobs[loop].blob;

				if ( b1->fc_stat ) {
					/* If status is not 0 then handle a few special, permissible cases */
					switch ( loop ) {
						case BMCHARGE:
							if ( FC_STAT_BPM_REFLO == b1->fc_stat )
								goto passed;
							break;

						case BMBUNCHLEN:
							if ( ! (FC_STAT_BLEN_INVAL_BIMAX_MASK & b1->fc_stat) )
								goto passed;
							break;

						default:
							break;
					}

					bldInvalidAlarmCount[loop]++;
					dataAvailable &= ~bldPulseBlobs[loop].aMsk;
				}
passed:

				/* verify timestamp */
				if ( 0 != pvTimePulseIdMatches( p_refTime, b1 ) ) {
					if ( loop == tsCatch ) {
						tsMismatch[0]=p_refTime->secPastEpoch;
						tsMismatch[1]=p_refTime->nsec;
						tsMismatch[2]=b1->fc_tsHi;
						tsMismatch[3]=b1->fc_tsLo;
						tsCatch = -1;
					}
					bldUnmatchedTSCount[loop]++;
					dataAvailable &= ~bldPulseBlobs[loop].aMsk;
				}
			}

			__st_le32(&bldEbeamInfo.ts_sec,      p_refTime->secPastEpoch);
			__st_le32(&bldEbeamInfo.ts_nsec,     p_refTime->nsec);
			__st_le32(&bldEbeamInfo.uFiducialId, PULSEID((*p_refTime)));

			/* Calculate beam charge */
			if( (dataAvailable & AVAIL_BMCHARGE) ) {
				__st_le64(&bldEbeamInfo.ebeamCharge, (double)bldPulseBlobs[BMCHARGE].blob->fcbl_bpm_T * 1.602e-10);
			} else {
				bldEbeamInfo.uDamageMask |= __le32(0x1);
			}

#define AVAIL_L3ENERGY (AVAIL_BMENERGY1X | AVAIL_BMENERGY2X | AVAIL_DSPR1 | AVAIL_DSPR2 | AVAIL_E0BDES)

			/* Calculate beam energy */
			if( AVAIL_L3ENERGY == (AVAIL_L3ENERGY & dataAvailable ) ) {
				double tempD;
				tempD = bldPulseBlobs[BMENERGY1X].blob->fcbl_bpm_X/(1000.0 * bldStaticPVs[DSPR1].pTD->value);
				tempD += bldPulseBlobs[BMENERGY2X].blob->fcbl_bpm_X/(1000.0 * bldStaticPVs[DSPR2].pTD->value);
				tempD = tempD/2.0 + 1.0;
				tempD *= bldStaticPVs[E0BDES].pTD->value * 1000.0;
				__st_le64(&bldEbeamInfo.ebeamL3Energy, tempD);
			} else {
				bldEbeamInfo.uDamageMask |= __le32(0x2);
			}

#define AVAIL_LTUPOS ( AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y | \
		AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y | \
		AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y | \
		AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y | \
		AVAIL_FMTRX )

			/* Calculate beam position */
			if( AVAIL_LTUPOS == (AVAIL_LTUPOS & dataAvailable) ) {
				dbr_double_t *pMatrixValue;
				double tempDA[4];
				int i;

				pMatrixValue = &(bldStaticPVs[FMTRX].pTD->value);
				for ( i=0; i<4; i++, pMatrixValue+=8 ) {
					double acc = 0.0;
#if 0
					for(j=0;j<4;j++) {
						tempDA[i] += pMatrixValue[i*8+2*j] * bldPulseBlobs[BMPOSITION1X+j].blob->fcbl_bpm_X;
						tempDA[i] += pMatrixValue[i*8+2*j+1] * bldPulseBlobs[BMPOSITION1X+j].blob->fcbl_bpm_Y;
					}
#else
					acc += pMatrixValue[0] * bldPulseBlobs[BMPOSITION1X].blob->fcbl_bpm_X;
					acc += pMatrixValue[1] * bldPulseBlobs[BMPOSITION1X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[2] * bldPulseBlobs[BMPOSITION2X].blob->fcbl_bpm_X;
					acc += pMatrixValue[3] * bldPulseBlobs[BMPOSITION2X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[4] * bldPulseBlobs[BMPOSITION3X].blob->fcbl_bpm_X;
					acc += pMatrixValue[5] * bldPulseBlobs[BMPOSITION3X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[6] * bldPulseBlobs[BMPOSITION4X].blob->fcbl_bpm_X;
					acc += pMatrixValue[7] * bldPulseBlobs[BMPOSITION4X].blob->fcbl_bpm_Y;

					tempDA[i] = acc;	
#endif
				}
				__st_le64(&bldEbeamInfo.ebeamLTUPosX, tempDA[0]);
				__st_le64(&bldEbeamInfo.ebeamLTUPosY, tempDA[1]);
				__st_le64(&bldEbeamInfo.ebeamLTUAngX, tempDA[2]);
				__st_le64(&bldEbeamInfo.ebeamLTUAngY, tempDA[3]);
			} else {
				bldEbeamInfo.uDamageMask |= __le32(0x3C);
			}

			/* Copy bunch length */
			if( (AVAIL_BMBUNCHLEN & dataAvailable) ) {
				__st_le64(&bldEbeamInfo.ebeamBunchLen, (double)bldPulseBlobs[BMBUNCHLEN].blob->fcbl_blen_bimax);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(0x40);
			}

			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}
			epicsMutexUnlock(bldMutex);
		}
#endif /* USE_PULSE_CA */

		scanIoRequest(bldIoscan);

#ifdef MULTICAST
		/* do MultiCast */
		if(BLD_MCAST_ENABLE)
		{
#ifdef MULTICAST_UDPCOMM
			rtncode = udpCommSend( sFd, &bldEbeamInfo, sizeof(bldEbeamInfo) );
			if ( rtncode < 0 ) {
				if ( BLD_MCAST_DEBUG )
					errlogPrintf("Sending multicast packet failed: %s\n", strerror(-rtncode));
			}
#else
			if(-1 == sendto(sFd, (void *)&bldEbeamInfo, sizeof(struct EBEAMINFO), 0, (const struct sockaddr *)&sockaddrDst, sizeof(struct sockaddr_in)))
			{
				if(BLD_MCAST_DEBUG) perror("Multicast sendto failed\n");
			}
#endif
			bldMcastMsgSent++;
		}
#endif
#ifndef USE_PULSE_CA
		for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
			if ( bldPulseBlobs[loop].blob ) {
				rtncode = fcomReleaseBlob( & bldPulseBlobs[loop].blob );
				if ( rtncode ) {
					errlogPrintf("Fatal Error: Unable to release blob (for %s): %s\n",
							bldPulseBlobs[loop].name,
							fcomStrerror(rtncode));
					return -1;
				}
			}
		}
#endif
	}

#ifdef MULTICAST
#ifdef MULTICAST_UDPCOMM
	udpCommClose( sFd );
#else
	close( sFd );
#endif
#endif

#ifndef USE_PULSE_CA
	for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
		fcomUnsubscribe( bldPulseBlobs[loop].fcid );
	}
#endif

	/*Should never return from following call*/
	/*SEVCHK(ca_pend_event(0.0),"ca_pend_event");*/
	return(0);
}

#ifndef USE_PULSE_CA
void
dumpTsMismatchStats(FILE *f, int reset)
{
int      i;
unsigned cnt [N_PULSE_BLOBS];
unsigned cnt1[N_PULSE_BLOBS];

	if ( bldMutex ) epicsMutexLock( bldMutex );
		memcpy(cnt,  bldUnmatchedTSCount,  sizeof(cnt));
		memcpy(cnt1, bldInvalidAlarmCount, sizeof(cnt1));
	if ( bldMutex ) epicsMutexUnlock( bldMutex );
	if ( !f )
		f = stdout;
	fprintf(f,"FCOM Data arriving late:\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9u times\n", bldPulseBlobs[i].name, bldUnmatchedTSCount[i]);
	}
	fprintf(f,"FCOM Data with bad/invalid status\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9u times\n", bldPulseBlobs[i].name, bldUnmatchedTSCount[i]);
	}
	fprintf(f,"MCAST messages posted:     %10u\n", bldMcastMsgSent);
	fprintf(f,"FCOM 'get' message errors: %10u\n", bldFcomGetErrs);
	if ( reset ) {
		if ( bldMutex ) epicsMutexLock( bldMutex );
			for ( i=0; i<N_PULSE_BLOBS; i++ ) {
				bldUnmatchedTSCount[i] = 0;
			}
			bldMcastMsgSent = 0;
		if ( bldMutex ) epicsMutexUnlock( bldMutex );
	}
}
#endif

/**************************************************************************************************/
/* Here we supply the access methods for 'devBusMapped'                                           */
/**************************************************************************************************/

static int
timer_delay_rd(DevBusMappedPvt pvt, unsigned *pvalue, dbCommon *prec)
{
uint64_t us;

	us = 1000000ULL * (uint64_t)timer_delay_clicks;
	us = us / (uint64_t)BSP_timer_clock_get(BSPTIMER);
	*pvalue = (unsigned)us;

	return 0;
}

static int
timer_delay_wr(DevBusMappedPvt pvt, unsigned value, dbCommon *prec)
{
uint64_t clicks;

	clicks = (uint64_t)BSP_timer_clock_get(BSPTIMER) * (uint64_t)value;	/* delay from fiducial in us */
	timer_delay_clicks = (uint32_t) (clicks / 1000000ULL);

	return 0;
}

static DevBusMappedAccessRec timer_delay_io = {
	rd: timer_delay_rd,
	wr: timer_delay_wr,
};

/**************************************************************************************************/
/* Here we supply the driver report function for epics                                            */
/**************************************************************************************************/
static long     BLD_EPICS_Init();
static  long    BLD_EPICS_Report(int level);

static const struct drvet drvBLD = {2,                              /*2 Table Entries */
                              (DRVSUPFUN) BLD_EPICS_Report,  /* Driver Report Routine */
                              (DRVSUPFUN) BLD_EPICS_Init}; /* Driver Initialization Routine */

epicsExportAddress(drvet,drvBLD);

static void
check(char *nm, void *ptr)
{
	if ( devBusMappedRegister(nm, ptr) ) {
		errlogPrintf("WARNING: Unable to register '%s'; related statistics may not work\n", nm);
	}
}

/* implementation */
static long BLD_EPICS_Init()
{
	if ( devBusMappedRegisterIO("bld_timer_io", &timer_delay_io) )
		errlogPrintf("ERROR: Unable to register I/O methods for timer delay\n"
                     "SOFTWARE MAY NOT WORK PROPERLY\n");
	check("bld_stat_bad_t", &bldUnmatchedTSCount);
	check("bld_stat_bad_d", &bldInvalidAlarmCount);
	check("bld_stat_bad_f", &bldFcomGetErrs);
	check("bld_stat_msg_s", &bldMcastMsgSent);
    BLDMCastStart(1, getenv("IPADDR1"));
    return 0;
}

static long BLD_EPICS_Report(int level)
{
    printf("\n"BLD_DRV_VERSION"\n\n");

    return 0;
}
