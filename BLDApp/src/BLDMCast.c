/* $Id: BLDMCast.c,v 1.67.2.3 2015/09/30 09:58:26 bhill Exp $ */
/*=============================================================================

  Name: BLDMCast.c

  Abs:  BLDMCast driver for eBeam MCAST BLD data sent to PCDS.

  Auth: Sheng Peng (pengs)
  Mod:	Till Straumann (strauman)
  		Luciano Piccoli (lpiccoli)
  		Shantha Condamoor (scondam)
  		Bruce Hill (bhill)

  Mod:  22-Sep-2009 - S.Peng - Initial Release
		18-May-2010 - T.Straumann: BLD-R2-0-0-BR - Cleanup and modifications
		12-May-2011 - L.Piccoli	- Modifications
		11-Jun-2013 - L.Piccoli	- BLD-R2-2-0 - 	Addition of BLD receiver - phase cavity  
		30-Sep-2013 - L.Piccoli - BLD-R2-3-0 - Addition of Fast Undulator Launch feedback states, version 0x4000f
		28-Feb-2014 - L.Piccoli - BLD-R2-4-0 - Merged BLD-R2-0-0-BR branch with MAIN_TRUNK. Addition of TCAV/DMP1 PVs to BLD. Version 0x5000f
		12-Mar-2014 - L.Piccoli - BLD-R2-5-4, BLD-R2-5-3, BLD-R2-5-2, BLD-R2-5-1, BLD-R2-5-0, BLD-R2-4-6 - Prevents BLDCast task on all iocs except ioc-sys0-bd01
		20-Jun-2014 - S.Condamoor - BLD-R2-5-5 - BLDSender and BLDReceiver apps have been split. bld_receivers_report() not needed for Sender	   
		7-Jul-2014  - S.Condamoor - BLD-R2-6-0 - Added Photon Energy Calculation to eBeam BLD MCAST data . Version 0x6000f
												Added code to set the 0x20000 damage bit if the EPICS variables become disconnected, or
												    if the BPM data is unavailable.
		11-Nov-2014: S.Condamoor - BLD-R2-6-2 L.Piccoli moved Und Launch Feeback to FB05:RF05. FBCK:FB03:TR05:STATES changed to FBCK:FB05:TR05:STATES	
		2-Feb-2015  - S.Condamoor - BLD-R2-6-3 - Fix for etax in Photon Energy Calculation per PCDS request. Version 0x7000f												
		18-Sep-2015 - B.Hill - Modified for linux compatibility
-----------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>

#include <epicsExport.h>
#include <epicsEvent.h>
#include <epicsThread.h>
#include <epicsTypes.h>
#include <cadef.h>
#include <drvSup.h>
#include <alarm.h>
#include <cantProceed.h>
#include <errlog.h>
#include <epicsExit.h>

#include "HiResTime.h"
#include "evrTime.h"
#include "evrPattern.h"

#include "fcom_api.h"
#include "fcomUtil.h"
#include "udpComm.h"
#include "fcomLclsBpm.h"
#include "fcomLclsBlen.h"
#include "fcomLclsLlrf.h"
#include "debugPrint.h"
extern int	fcomUtilFlag;

#include "devBusMapped.h"
#ifdef RTEMS
#include <bsp/gt_timer.h> /* This is MVME6100/5500 only */
#endif

#include "BLDMCast.h"

#define BLD_DRV_VERSION "BLD driver $Revision: 1.67.2.3 $/$Name: RT-devel $"

#define CA_PRIORITY     CA_PRIORITY_MAX         /* Highest CA priority */

#define TASK_PRIORITY   epicsThreadPriorityMax  /* Highest EPICS thread priority */

#define DEFAULT_CA_TIMEOUT	0.04		/* Default CA timeout, for 30Hz */
#define DEFAULT_EVR_TIMEOUT	0.2		/* Default EVR event timeout, for 30Hz */

#define MCAST_TTL	8

#define	CONNECT_PV
#undef  USE_CA_ADD_EVENT    /* ca_create_subscription seems buggy */
#undef  USE_PULSE_CA        /* Use CA for obtaining pulse PVs; use FCOM otherwise */
/* T.S: I doubt that ca_create_subscription works any different. 
 * ca_add_array_event is just a macro mapping to the
 * ca_add_masked_array_event() wrapper which calls
 * ca_create_subscription with the DBE_VALUE | DBE_ALARM mask ...
 */
#define MULTICAST           /* Use multicast interface */
#define MULTICAST_UDPCOMM   /* Use UDPCOMM for data output; BSD sockets otherwise */

/*
#ifdef FB05_TEST / * If running of FB05, disable multicast - this is already done by the BLD IOC * /
#undef MULTICAST
#undef MULTICAST_UDPCOMM
#endif
*/

#define BSPTIMER    0       /* Timer instance -- use first timer */

#ifndef FID_DIFF
/*
 *  A few fiducial helper definitions.
 *  FID_ROLL(a, b) is true if we have rolled over from fiducial a to fiducial b.  (That is, a
 *  is large, and b is small.)
 *  FID_GT(a, b) is true if fiducial a is greater than fiducial b, accounting for rollovers.
 *  FID_DIFF(a, b) is the difference between two fiducials, accounting for rollovers.
 */
#define FID_MAX        0x1ffe0
#define FID_ROLL_LO    0x00200
#define FID_ROLL_HI    (FID_MAX-FID_ROLL_LO)
#define FID_ROLL(a,b)  ((b) < FID_ROLL_LO && (a) > FID_ROLL_HI)
#define FID_GT(a,b)    (FID_ROLL(b, a) || ((a) > (b) && !FID_ROLL(a, b)))
#define FID_DIFF(a,b)  ((FID_ROLL(b, a) ? FID_MAX : 0) + (int)(a) - (int)(b) - (FID_ROLL(a, b) ? FID_MAX : 0))
#endif	/* FID_DIFF */

/* We monitor static PVs and update their value upon modification */
enum STATICPVSINDEX
{
    DSPR1=0,
    DSPR2,
    E0BDES,
    FMTRX,
/* Shantha Condamoor: 7-Jul-2014: The following 3 are for matlab PVs that are used in shot-to-shot photon energy calculations*/	
	PHOTONEV,
	X450AVE,
	X250AVE
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
    BC2CHARGE,
    BC2ENERGY,
    BC1CHARGE,
    BC1ENERGY,
    UNDSTATE, /* For X, X', Y and Y' */
    BMDMPCHARGE,
    XTCAVRF,
	/* Define Y after everything else so that fcom array doesn't have to use them */
    BMPOSITION1Y,
    BMPOSITION2Y,
    BMPOSITION3Y,
    BMPOSITION4Y
};/* the definition here must match the PV definition below, the order is critical as well */

/*
  [BMCHARGE] 
  [BMENERGY1X]
  [BMENERGY2X]
  [BMPOSITION1X]
  [BMPOSITION2X]
  [BMPOSITION3X]
  [BMPOSITION4X]
  [BC2CHARGE]   
  [BC2ENERGY]   
  [BC1CHARGE]   
  [BC1ENERGY]   
  [UNDSTATE]    
  [BMDMPCHARGE] 
  [XTCAVRF]  
*/

/** Former PVAVAILMASK enum */
#define AVAIL_DSPR1          0x0001
#define AVAIL_DSPR2          0x0002
#define AVAIL_E0BDES         0x0004
#define AVAIL_FMTRX          0x0008
#define AVAIL_BMCHARGE       0x0010
#define AVAIL_BMENERGY1X     0x0020
#define AVAIL_BMENERGY2X     0x0040
#define AVAIL_BMPOSITION1X   0x0080
#define AVAIL_BMPOSITION2X   0x0100
#define AVAIL_BMPOSITION3X   0x0200
#define AVAIL_BMPOSITION4X   0x0400
#define AVAIL_BMPOSITION1Y   0x0800
#define AVAIL_BMPOSITION2Y   0x1000
#define AVAIL_BMPOSITION3Y   0x2000
#define AVAIL_BMPOSITION4Y   0x4000
#define AVAIL_BC2CHARGE      0x8000
#define AVAIL_BC2ENERGY     0x10000
#define AVAIL_BC1CHARGE     0x20000
#define AVAIL_BC1ENERGY     0x40000
#define AVAIL_UNDSTATE      0x80000
#define AVAIL_DMPCHARGE    0x100000
#define AVAIL_XTCAVRF      0x200000

/* Shantha Condamoor: 7-Jul-2014: The following are for matlab PVs that are used in shot-to-shot photon energy calculations*/
#define AVAIL_PHOTONEV     0x400000
#define AVAIL_X450AVE      0x800000
#define AVAIL_X250AVE     0x1000000

/* Structure representing one PV (= channel) */
typedef struct BLDPV
{
  const char *	         name;
  unsigned long	         nElems; /* type is always DOUBLE */
  
  unsigned int	         availMask;

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

typedef struct BLDBLOB
{
	const char *            name;
	FcomBlobRef             blob;
	long                    aMsk;
} BLDBLOB;


#undef IGNORE_UNAVAIL_PVS
BLDPV bldStaticPVs[]=
{
    [DSPR1]  = {"BLD:SYS0:500:DSPR1",  1, AVAIL_DSPR1,  NULL, NULL},	/* For Energy */
    [DSPR2]  = {"BLD:SYS0:500:DSPR2",  1, AVAIL_DSPR2,  NULL, NULL},	/* For Energy */
#ifndef IGNORE_UNAVAIL_PVS
    [E0BDES] = {"BEND:LTU0:125:BDES",  1, AVAIL_E0BDES, NULL, NULL},	/* Energy in MeV */
#endif /* IGNORE_UNAVAIL_PVS */
    [FMTRX]  = {"BLD:SYS0:500:FMTRX", 32, AVAIL_FMTRX,  NULL, NULL},		/* For Position */
/* Shantha Condamoor: 7-Jul-2014: The following are for matlab PVs that are used in shot-to-shot photon energy calculations*/	
#ifndef IGNORE_UNAVAIL_PVS
	[PHOTONEV]={"SIOC:SYS0:ML00:AO627",1, AVAIL_PHOTONEV,  NULL, NULL},	/* For shot-to-shot Photon Energy */
#endif /* IGNORE_UNAVAIL_PVS */
	[X450AVE]= {"SIOC:SYS0:ML02:AO041",1, AVAIL_X450AVE,  NULL, NULL},	/* Average of last few hundred data points of X POS in LTU BPM x450 */		
	[X250AVE]= {"SIOC:SYS0:ML02:AO040",1, AVAIL_X250AVE,  NULL, NULL},	/* Average of last few hundred data points of X POS in LTU BPM x250 */	
};

#define N_STATIC_PVS (sizeof(bldStaticPVs)/sizeof(bldStaticPVs[0]))

BLDBLOB bldPulseBlobs[] =
{
  /**
   * Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
   */
  [BMCHARGE] = { name: "BPMS:IN20:221:TMIT", blob: 0, aMsk: AVAIL_BMCHARGE},	/* Charge in Nel, 1.602e-10 nC per Nel*/

  /**
   * Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
   * where E0 is the final desired energy at the LTU (the magnet setting BEND:LTU0:125:BDES*1000)
   * dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
   * BPM1x = [BPMS:LTU1:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
   * BPM2x = [BPMS:LTU1:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
   */
    [BMENERGY1X] = { name: "BPMS:LTU1:250:X", blob: 0, aMsk: AVAIL_BMENERGY1X},	/* Energy in MeV */
    [BMENERGY2X] = { name: "BPMS:LTU1:450:X", blob: 0, aMsk: AVAIL_BMENERGY2X},	/* Energy in MeV */

  /**
   * Position X, Y, Angle X, Y at LTU:
   * Using the LTU Feedback BPMs: BPMS:LTU1:720,730,740,750
   * The best estimate calculation is a matrix multiply for the result vector p:
   *
   *        [xpos(mm)             [bpm1x         //= BPMS:LTU1:720:X (mm)
   *    p=   ypos(mm)      = [F]*  bpm2x         //= BPMS:LTU1:730:X
   *         xang(mrad)            bpm3x         //= BPMS:LTU1:740:X
   *         yang(mrad)]           bpm4x         //= BPMS:LTU1:750:X
   *                               bpm1y         //= BPMS:LTU1:720:Y
   *                               bpm2y         //= BPMS:LTU1:730:Y
   *                               bpm3y         //= BPMS:LTU1:740:Y
   *                               bpm4y]        //= BPMS:LTU1:750:Y
   *
   *    Where F is the 4x8 precalculated fit matrix.  F is approx. pinv(A), for Least Squares fit p = pinv(A)*x
   *
   *    A = [R11 R12 R13 R14;     //rmat elements for bpm1x
   *         R11 R12 R13 R14;     //rmat elements for bpm2x
   *         R11 R12 R13 R14;     //rmat elements for bpm3x
   *         R11 R12 R13 R14;     //rmat elements for bpm4x
   *         R31 R32 R33 R34;     //rmat elements for bpm1y
   *         R31 R32 R33 R34;     //rmat elements for bpm2y
   *         R31 R32 R33 R34;     //rmat elements for bpm3y
   *         R31 R32 R33 R34]     //rmat elements for bpm4y
   */
  [BMPOSITION1X] = { name: "BPMS:LTU1:720:X"    , blob: 0, aMsk: AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y },	/* Position in mm/mrad */
  [BMPOSITION2X] = { name: "BPMS:LTU1:730:X"    , blob: 0, aMsk: AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y },	/* Position in mm/mrad */
  [BMPOSITION3X] = { name: "BPMS:LTU1:740:X"    , blob: 0, aMsk: AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y },	/* Position in mm/mrad */
  [BMPOSITION4X] = { name: "BPMS:LTU1:750:X"    , blob: 0, aMsk: AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y },	/* Position in mm/mrad */
  [BC2CHARGE]    = { name: "BLEN:LI24:886:BIMAX", blob: 0, aMsk: AVAIL_BC2CHARGE },	/* BC2 Charge in Amps */
  [BC2ENERGY]    = { name: "BPMS:LI24:801:X"    , blob: 0, aMsk: AVAIL_BC2ENERGY },	/* BC2 Energy in mm */
  [BC1CHARGE]    = { name: "BLEN:LI21:265:AIMAX", blob: 0, aMsk: AVAIL_BC1CHARGE },	/* BC1 Charge in Amps */
  [BC1ENERGY]    = { name: "BPMS:LI21:233:X"    , blob: 0, aMsk: AVAIL_BC1ENERGY },	/* BC1 Energy in mm */
  
  /**
   * Undulator Launch 120Hz Feedback States X, X', Y, Y' (running on FB03:TR05)
   * scondam: 11-Nov-2014: L.Piccoli moved Und Launch Feeback to FB05:TR05
   * [UNDSTATE]     = { name: "FBCK:FB03:TR05:STATES", blob: 0, aMsk: AVAIL_UNDSTATE},    
   */
  [UNDSTATE]     = { name: "FBCK:FB05:TR05:STATES", blob: 0, aMsk: AVAIL_UNDSTATE}, 

  /**
   * Charge at the DMP
   */
  [BMDMPCHARGE]     = { name: "BPMS:DMP1:502:TMIT", blob: 0, aMsk: AVAIL_DMPCHARGE}, 

  /**
   * XTCAV Voltage and Phase
   */
  [XTCAVRF]  = { name: "TCAV:DMP1:360:AV", blob: 0, aMsk: AVAIL_XTCAVRF},
  
};


#define N_PULSE_BLOBS (sizeof(bldPulseBlobs)/sizeof(bldPulseBlobs[0]))

FcomID bldPulseID[N_PULSE_BLOBS] = { 0 };

FcomBlobSetRef  bldBlobSet = 0;


static epicsInt32 bldUseFcomSet = 0;

static unsigned int dataAvailable = 0;

int BLD_MCAST_ENABLE = 1;
epicsExportAddress(int, BLD_MCAST_ENABLE);

int BLD_MCAST_DEBUG = 1;
epicsExportAddress(int, BLD_MCAST_DEBUG);

int BLD_XTCAV_DEBUG = 0;
epicsExportAddress(int, BLD_XTCAV_DEBUG);

static uint32_t timer_delay_clicks = 10 /* just some value; true thing is written by EPICS record */;
static uint32_t timer_delay_ms     =  4 /* just some value; true thing is written by EPICS record */;

int DELAY_FOR_CA = 0;	/* in seconds */
epicsExportAddress(int, DELAY_FOR_CA);

/* Share with device support */
volatile int bldAllPVsConnected   = 0;
volatile int bldConnectAbort      = 1;
epicsExportAddress(int, bldConnectAbort);
epicsUInt32  bldInvalidAlarmCount[N_PULSE_BLOBS] = {0};
epicsUInt32  bldUnmatchedTSCount[N_PULSE_BLOBS]  = {0};
EBEAMINFO    bldEbeamInfo;
IOSCANPVT    bldIoscan;         /* Trigger EPICS record */
epicsMutexId bldMutex = NULL;	/* Protect bldStaticPVs' values */
epicsUInt32  bldFcomGetErrs[N_PULSE_BLOBS]       = {0};
epicsUInt32  bldMcastMsgSent                     =  0;
epicsUInt32  bldMinFcomDelayUs                   = -1;
epicsUInt32  bldAvgFcomDelayUs                   =  0;
epicsUInt32  bldMaxFcomDelayUs                   =  0;
epicsUInt32  bldMaxPostDelayUs                   =  0;
epicsUInt32  bldAvgPostDelayUs                   =  0;

/* Share with device support */

/*==========================================================*/
static in_addr_t mcastIntfIp = 0;

static epicsEventId EVRFireEvent = NULL;

/* EPICS time has pulse ID in lower bits of nanoseconds;
 * for measuring high-resolution delays we also need
 * the real nano- or at least microseconds.
 */
epicsTimeStamp  bldFiducialTime;
t_HiResTime		bldFiducialTsc = 0LL;	/* 64 bit hi-res cpu timestamp counter */

int      tsCatch = -1;
uint32_t tsMismatch[4];

static epicsUInt32
usSinceFiducial(void)
{
	/* Note: The hi-res fiducial timestamp is only set on beam fiducials */
	t_HiResTime		curTick		= GetHiResTicks();
	epicsUInt32		usSinceFid	= HiResTicksToSeconds( curTick - bldFiducialTsc ) * 1e6;
	return usSinceFid;
}

#ifdef RTEMS
static void evr_timer_isr(void *arg);
#endif
static int BLDMCastTask(void * parg);
static void BLDMCastTaskEnd(void * parg);

void BLDMCastStart(int enable, const char * NIC)
{
	/* This function will be called in st.cmd after iocInit() */
    printf( "BLDMCastStart: %s %s\n", (enable ? "enable" : "disable"), NIC );
    BLD_MCAST_ENABLE = enable;

    if(NIC && NIC[0] != 0)
        mcastIntfIp = inet_addr(NIC);

    if(mcastIntfIp == (in_addr_t)(-1))
    {
	errlogPrintf("Illegal MultiCast NIC IP\n");
	return;
    }

#ifdef RTEMS
    /************Setup high resolution timer ***************************************************************/
    /* you need to setup the timer only once (to connect ISR) */
    BSP_timer_setup( BSPTIMER, evr_timer_isr, 0, 0 /* do not reload timer when it expires */);
#endif

    epicsAtExit(BLDMCastTaskEnd, NULL);

	epicsThreadMustCreate("BLDMCast", TASK_PRIORITY, 20480, (EPICSTHREADFUNC)BLDMCastTask, NULL);
    return;
}

static int
pvTimePulseIdMatches(
	epicsTimeStamp *p_ref,
	FcomBlobRef p_cmp
)
{
epicsUInt32 idref, idcmp, diff;

	idref = PULSEID((*p_ref));
	idcmp = p_cmp->fc_tsLo & LOWER_17_BIT_MASK;

	if ( idref == idcmp && idref != PULSEID_INVALID ) {
		/* Verify that seconds match to less that a few minutes */
		diff = abs( p_ref->secPastEpoch - p_cmp->fc_tsHi );
		if ( diff < 4*60 )
			return 0;
	}

	return -1;
}

/*
 * This function will be registered with EVR for callback each fiducial at 360hz
 * The argument is the bldBlobSet ptr allocated by fcomAllocBlobSet()
 */
void EVRFire(void *use_sets)
{
	/* evrRWMutex is locked while calling these user functions so don't do anything that might block. */
	epicsTimeStamp time_s;

	/* get the current pattern data - check for good status */
	evrModifier_ta modifier_a;
	epicsUInt32  patternStatus; /* see evrPattern.h for values */
	int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
	/* This is 120Hz. So printf will screw timing */
	if(BLD_MCAST_DEBUG >= 4) errlogPrintf("EVR fires (status %i, mod5 0x%08x)\n", status, (unsigned)modifier_a[4]);
	if ( status != 0 )
	{
		/* Error from evrTimeGetFromPipeline! */
		bldFiducialTime.nsec = PULSEID_INVALID;
		return;
	}

#ifndef RTEMS
	int curFid	= PULSEID( time_s );
	int beamFid	= PULSEID( bldFiducialTime );
	if ( FID_DIFF( curFid, beamFid ) == 2 )
	{
		/* No BSP timer, so we fire fCom collection 2 fids after beam */
		epicsEventSignal( EVRFireEvent);
	}
#endif

	/* check for LCLS beam and rate-limiting */
	if ( (modifier_a[4] & MOD5_BEAMFULL_MASK) == 0 )
	{
		/* No beam */
		return;
	}

	/* Get timestamps for beam fiducial */
	bldFiducialTime = time_s;
	bldFiducialTsc	= GetHiResTicks();

#ifdef RTEMS
	if ( use_sets == NULL ) {
		BSP_timer_start( BSPTIMER, timer_delay_clicks );
	}
#endif
	return;
}

#ifdef RTEMS
/* Invoked by BSP_timer on 5ms timeout after fiducial */
/* Wakes up BLDMCast thread which calls fcomGetBlobSet() */
static void evr_timer_isr(void *arg)
{/* post event/release sema to wakeup worker task here */
  if(EVRFireEvent) { 
    epicsEventSignal(EVRFireEvent);
  }
    return;
}
#endif

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

static void eventCallback(struct event_handler_args args)
{
    BLDPV * pPV = args.usr;

    if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Event Callback: %s, status %d\n", ca_name(args.chid), args.status);

    epicsMutexLock(bldMutex);
    if (args.status == ECA_NORMAL)
    {
        memcpy(pPV->pTD, args.dbr, dbr_size_n(args.type, args.count));
        if(pPV->pTD->severity >= INVALID_ALARM)	/* We don't care timestamp for bldStaticPVs. As long as it is not invalid, they are ok */
			dataAvailable &= ~ pPV->availMask;
        else
			dataAvailable |=   pPV->availMask;

        if(BLD_MCAST_DEBUG >= 3)
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


#ifdef CONNECT_PV
static int
connectCaPv(const char *name, chid *p_chid)
{
int rtncode;

	do {

		rtncode = ECA_NORMAL;

		/* No need for connectionCallback since we want to wait till connected */
		if ( BLD_MCAST_DEBUG >= 1 ) {
			printf("creating channel for %s ...\n", name ); fflush(stdout);
		}

		SEVCHK(ca_create_channel(name, NULL/*connectionCallback*/, NULL, CA_PRIORITY , p_chid), "ca_create_channel");

		if ( BLD_MCAST_DEBUG >= 2 ) {
			printf(" ca_create_channel successful for %s\n", name );
			fflush(stdout);
		}

		/* Not very necessary */
		/* SEVCHK(ca_replace_access_rights_event(bldStaticPVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event"); */

		/* We could do subscription in connetion callback. But in this case, better to enforce all connection */
		if ( BLD_MCAST_DEBUG >= 2 ) {
			printf("pending for IO ...\n");
			fflush(stdout);
		}

		rtncode = ca_pend_io(10.0);

		if ( ECA_NORMAL != rtncode ) {

			ca_clear_channel(*p_chid);
			*p_chid = 0;

			if (rtncode == ECA_TIMEOUT) {

				errlogPrintf("Channel connect timed out: '%s' not found.\n", name);
				fflush(stdout);
				if ( ! bldConnectAbort ) {
					errlogPrintf("Continuing to try -- set bldConnectAbort nonzero to abort\n");
					continue;
				}
			} else {
				errlogPrintf("ea_pend_io() returned %i\n", rtncode);
			}
			return -1;
		}

	} while ( ECA_TIMEOUT == rtncode );

	return 0;
}
#endif /* CONNECT_PV */

static int
init_pvarray(BLDPV *p_pvs, int n_pvs, int subscribe)
{
	int           loop;
	unsigned long cnt;
	unsigned int	nFailedConnections = 0;

	for(loop=0; loop<n_pvs; loop++)
	{
#ifndef CONNECT_PV
		printf( "init_pvarray disabled.  Not connecting to PV %s\n", p_pvs[loop].name );
#else
		if ( connectCaPv( p_pvs[loop].name, &p_pvs[loop].pvChId ) ) {
			nFailedConnections++;
			continue;
		}

		if ( p_pvs[loop].nElems != (cnt = ca_element_count(p_pvs[loop].pvChId)) ) {
			errlogPrintf("Number of elements [%ld] of '%s' does not match expectation.\n", cnt, p_pvs[loop].name);
			return -1;
		}

		if ( DBF_DOUBLE != ca_field_type(p_pvs[loop].pvChId) ) {/* Native data type has to be double */
			errlogPrintf("Native data type of '%s' is not double.\n", p_pvs[loop].name);
			return -1;
		}

		/* Everything should be double, even not, do conversion */
		p_pvs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, p_pvs[loop].nElems), "callocMustSucceed");

		if ( subscribe ) {
#ifdef USE_CA_ADD_EVENT
			SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, p_pvs[loop].nElems, p_pvs[loop].pvChId, eventCallback, &(p_pvs[loop]), 0.0, 0.0, 0.0, NULL), "ca_add_array_event");
#else
			SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, p_pvs[loop].nElems, p_pvs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(p_pvs[loop]), NULL), "ca_create_subscription");
#endif
		}
#endif
	}

	if ( nFailedConnections > 5 ) {
		errlogPrintf("init_pvarray error: %d failed connection attempts!\n", nFailedConnections );
		return -1;
	}

	return 0;
}

static void BLDMCastTaskEnd(void * parg)
{
    int loop;

    bldAllPVsConnected = 0;
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

FcomBlobSetMask got_mask;

epicsUInt32     this_time;

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

	if ( DELAY_FOR_CA > 0 ) {
		printf("Delay %d seconds to wait CA ready!\n", DELAY_FOR_CA);
		printf("TODO: Try InitHookRegister here!\n" );
		for(loop=DELAY_FOR_CA;loop>0;loop--) {
			printf("\r%d seconds left!\n", loop);
			epicsThreadSleep(1.0);
		}
		printf("\n");
	}

	SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");
	SEVCHK(ca_add_exception_event(exceptionCallback,NULL), "ca_add_exception_event");		

	/* TODO, clean up resource when fail */

	/* Monitor static PVs */
	if ( init_pvarray( bldStaticPVs, N_STATIC_PVS, 1 /* do subscribe */ ) )
		return -1; /* error message already printed */	

	ca_flush_io();
	/* ca_pend_event(2.0); */		

	/* All ready to go, create event and register with EVR */
	EVRFireEvent = epicsEventMustCreate(epicsEventEmpty);
		
	/* Register EVRFire */
	evrTimeRegister(EVRFire, bldBlobSet);

	bldAllPVsConnected       = 1;
	printf("All PVs are successfully connected!\n");

	/* Prefill EBEAMINFO with constant values */
	bldEbeamInfo.uMBZ1       = __le32(0);
	bldEbeamInfo.uMBZ2       = __le32(0);

	bldEbeamInfo.uLogicalId  = __le32(0x06000000);
	bldEbeamInfo.uPhysicalId = __le32(0);
	bldEbeamInfo.uDataType   = __le32(EBEAMINFO_VERSION_5);
	bldEbeamInfo.uExtentSize = __le32(EBEAMINFO_VERSION_5_SIZE);

	bldEbeamInfo.uLogicalId2 = __le32(0x06000000);
	bldEbeamInfo.uPhysicalId2= __le32(0);
	bldEbeamInfo.uDataType2  = __le32(EBEAMINFO_VERSION_5);
	bldEbeamInfo.uExtentSize2= __le32(EBEAMINFO_VERSION_5_SIZE);		

	while(bldAllPVsConnected)
	{
		int status;

		{
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
		}

		/* Use FCOM to obtain pulse-by-pulse data */
		if ( bldBlobSet )
		{
			/* This is 30Hz. So printf might screw timing */
			if(BLD_MCAST_DEBUG >= 4) errlogPrintf("Calling fcomGetBlobSet w/ timeout %u ms\n", (unsigned int) timer_delay_ms );

			/* Get all the blobs w/ timeout timer_delay_ms */
			status = fcomGetBlobSet( bldBlobSet, &got_mask, (1<<N_PULSE_BLOBS) - 1, FCOM_SET_WAIT_ALL, timer_delay_ms );
			if ( status && FCOM_ERR_TIMEDOUT != status )
			{
				errlogPrintf("fcomGetBlobSet failed: %s; sleeping for 2 seconds\n", fcomStrerror(status));
				for ( loop = 0; loop < N_PULSE_BLOBS; loop++ )
					bldFcomGetErrs[loop]++;
				epicsThreadSleep( 2.0 );
				continue;
			}
			if ( status == FCOM_ERR_TIMEDOUT )
			{
				/* If a timeout happened then fall through; there still might be good
				 * blobs...
				 */
				if(BLD_MCAST_DEBUG >= 3) errlogPrintf("fcomGetBlobSet %u ms timeout!\n", (unsigned int) timer_delay_ms );
			}
			else
			{
				if(BLD_MCAST_DEBUG >= 4) errlogPrintf("fcomGetBlobSet succeeeded.\n");
			}
		}
		else
		{
			errlogPrintf( "fcomGetBlobSet failed: %s; sleeping for 2 seconds\n", fcomStrerror(status));
		}

		this_time = usSinceFiducial();

		if ( this_time > bldMaxFcomDelayUs )
			bldMaxFcomDelayUs = this_time;
		if ( this_time < bldMinFcomDelayUs )
			bldMinFcomDelayUs = this_time;

		/* Running average: fn+1 = 127/128 * fn + 1/128 * x = fn - 1/128 fn + 1/128 x */
		bldAvgFcomDelayUs +=  ((int)(- bldAvgFcomDelayUs + this_time)) >> 8;

		{
			FcomBlobRef b1;

			p_refTime = &bldFiducialTime;

			epicsMutexLock(bldMutex);

			bldEbeamInfo.uDamageMask = __le32(0);

			for ( loop = 0; loop < N_PULSE_BLOBS; loop++, got_mask >>= 1 ) {

				dataAvailable |= bldPulseBlobs[loop].aMsk;

				if ( bldBlobSet ) {
					/* NOTE: this could be coded more elegantly but
					 *       we want to share as much of the code path
					 *       that doesn't use a 'set' as possible.
					 */
					rtncode = ! (got_mask & 1);
					bldPulseBlobs[loop].blob = bldBlobSet->memb[loop].blob;
					/* If there was no data then it was probably too late */
					if ( rtncode )
						bldUnmatchedTSCount[loop]++;
				} else {
					rtncode = fcomGetBlob( bldPulseID[loop], &bldPulseBlobs[loop].blob, 0 );
					if ( rtncode )
						bldFcomGetErrs[loop]++;
				}

				if ( rtncode ) {
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

						case BC2CHARGE:
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
				bldEbeamInfo.uDamageMask |= __le32(EBEAMCHARGE_DAMAGEMASK);
			}

#define AVAIL_L3ENERGY (AVAIL_BMENERGY1X | AVAIL_BMENERGY2X | AVAIL_DSPR1 | AVAIL_DSPR2 | AVAIL_E0BDES)

			/* Calculate beam energy */
			if( AVAIL_L3ENERGY == (AVAIL_L3ENERGY & dataAvailable ) ) {
				double tempD = 0.0;
								
				if (bldStaticPVs[DSPR1].pTD->value != 0)
					tempD = bldPulseBlobs[BMENERGY1X].blob->fcbl_bpm_X/(1000.0 * bldStaticPVs[DSPR1].pTD->value);
				if (bldStaticPVs[DSPR2].pTD->value != 0)
					tempD += bldPulseBlobs[BMENERGY2X].blob->fcbl_bpm_X/(1000.0 * bldStaticPVs[DSPR2].pTD->value);
				tempD = tempD/2.0 + 1.0;
				tempD *= bldStaticPVs[E0BDES].pTD->value * 1000.0;
				__st_le64(&bldEbeamInfo.ebeamL3Energy, tempD);
					
				
			} else {
				bldEbeamInfo.uDamageMask |= __le32(EBEAML3ENERGY_DAMAGEMASK);
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
				bldEbeamInfo.uDamageMask |= __le32(EBEAMLTUPOSX_DAMAGEMASK | EBEAMLTUPOSY_DAMAGEMASK | EBEAMLTUANGX_DAMAGEMASK | EBEAMLTUANGY_DAMAGEMASK);
			}

			/* Copy BC2 Charge */
			if( AVAIL_BC2CHARGE & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamBC2Current, (double)bldPulseBlobs[BC2CHARGE].blob->fcbl_blen_bimax);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMBC2CURRENT_DAMAGEMASK);
			}

			/* Copy BC2 Energy */
			if( AVAIL_BC2ENERGY & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamBC2Energy, (double)bldPulseBlobs[BC2ENERGY].blob->fcbl_bpm_X);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMBC2ENERGY_DAMAGEMASK);
			}

			/* BC1 Charge */
			if( AVAIL_BC1CHARGE & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamBC1Current, (double)bldPulseBlobs[BC1CHARGE].blob->fcbl_blen_aimax);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMBC1CURRENT_DAMAGEMASK);
			}

			/* BC1 Energy */
			if( AVAIL_BC1ENERGY & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamBC1Energy, (double)bldPulseBlobs[BC1ENERGY].blob->fcbl_bpm_X);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMBC1ENERGY_DAMAGEMASK);
			}


			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}

			/* Undulator Launch 120Hz feedback states, X, X', Y and Y' */
			if( AVAIL_UNDSTATE & dataAvailable ) {
			  __st_le64(&bldEbeamInfo.ebeamUndPosX, (double)bldPulseBlobs[UNDSTATE].blob->fc_flt[0]);
			  __st_le64(&bldEbeamInfo.ebeamUndAngX, (double)bldPulseBlobs[UNDSTATE].blob->fc_flt[1]);
			  __st_le64(&bldEbeamInfo.ebeamUndPosY, (double)bldPulseBlobs[UNDSTATE].blob->fc_flt[2]);
			  __st_le64(&bldEbeamInfo.ebeamUndAngY, (double)bldPulseBlobs[UNDSTATE].blob->fc_flt[3]);
			  /*__st_le64(&bldEbeamInfo.ebeamCharge, (double)PULSEID((*p_refTime)));*/
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMUNDPOSX_DAMAGEMASK | EBEAMUNDPOSY_DAMAGEMASK | EBEAMUNDANGX_DAMAGEMASK | EBEAMUNDANGY_DAMAGEMASK);
			}

			/* XTCAV Phase & Amplitude */ 
			if( AVAIL_XTCAVRF & dataAvailable )
			{
			  if(BLD_XTCAV_DEBUG >= 1) {
			    errlogPrintf("Got XTCAV blob %f %f\n", (double)bldPulseBlobs[XTCAVRF].blob->fcbl_llrf_aavg,
					 (double)bldPulseBlobs[XTCAVRF].blob->fcbl_llrf_pavg);
			  }

			  __st_le64(&bldEbeamInfo.ebeamXTCAVAmpl, (double)bldPulseBlobs[XTCAVRF].blob->fcbl_llrf_aavg);
			  __st_le64(&bldEbeamInfo.ebeamXTCAVPhase, (double)bldPulseBlobs[XTCAVRF].blob->fcbl_llrf_pavg);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMXTCAVAMPL_DAMAGEMASK);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMXTCAVPHASE_DAMAGEMASK);
			}


			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}


			/* DMP Charge */ 
			if( AVAIL_DMPCHARGE & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamDMP502Charge, (double)bldPulseBlobs[BMDMPCHARGE].blob->fcbl_bpm_T);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMDMP502CHARGE_DAMAGEMASK);
			}


			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);								
			}
			
			/* 	Shantha Condamoor: 7-Jul-2014: Calculate shot-to-shot photon energy */			
			/* Algorithm from J.Welch and H.Loos */

			/* function eV = photonEnergyeV_BLD(x450, x250)

				calculates the accurate shot to shot SASE central photon energy given the
				shot to shot bpm positions in DL2
				x450 corresponds to the bpm x position data  from BPMS:LTU1:450:X
				x250 corresponds to the bpm x position data  from BPMS:LTU1:250:X

				BPMS:LTU1:450:X and BPMS:LTU1:250:X both arrive on every pulse over FCOM.
	
				eVave = SIOC:SYS0:ML00:AO627;      % a single number - Slow update (1 sec or so) over CA from matlab and from subroutine record
				
				% Slow update (1 sec or so) over CA from matlab - typically there should be the last few hundred data points in x450 or x250
				x450ave = SIOC:SYS0:ML02:AO041;    % from subroutine record
				x250ave = SIOC:SYS0:ML02:AO040;	   % from subroutine record
				x450 = BPMS:LTU1:450:X;
                x250 = BPMS:LTU1:250:X;
	
				etax = -125 ;       %  [mm] +/- design value for dispersion at bpms in dogleg
	
        		eVdelta = eVave * ( (x450 - x450ave) - (x250 - x250ave))/ (etax); % The two BPM positions get subtracted, not added, as they have opposite dispersion.
		
				eV = eVave + eVdelta;  % Send out on eBeam BLD data on every pulse in 'ebeamPhotonEnergy' field in eV */

#define AVAIL_PHOTONENERGY (AVAIL_BMENERGY1X | AVAIL_BMENERGY2X | AVAIL_PHOTONEV | AVAIL_X450AVE | AVAIL_X250AVE)	

			/* Calculate shot-to-shot photon energy */
			if( AVAIL_PHOTONENERGY == (AVAIL_PHOTONENERGY & dataAvailable ) ) {			
								
				double etax = -125; 
				/* shot-to-shot photon energy is calculated using following variables which arrive via CA from matlab */
				double eVave = bldStaticPVs[PHOTONEV].pTD->value;	/* SIOC:SYS0:ML00:AO627 - a single number - Slow update (1 sec or so) over CA from matlab */
				double x450ave = bldStaticPVs[X450AVE].pTD->value;	/* SIOC:SYS0:ML02:AO041 - Slow update (1 sec or so) over CA from matlab - typically there should be the last few hundred data points in x450 */												
				double x250ave = bldStaticPVs[X250AVE].pTD->value;	/* SIOC:SYS0:ML02:AO040 - Slow update (1 sec or so) over CA from matlab - typically there should be the last few hundred data points in x250 */

				/* following variables arrive on every pulse via FCOM */
				double x450 = bldPulseBlobs[BMENERGY2X].blob->fcbl_bpm_X;	/* BPMS:LTU1:450:X */
				double x250 = bldPulseBlobs[BMENERGY1X].blob->fcbl_bpm_X;	/* BPMS:LTU1:250:X */
				double eVdelta =  eVave * ( (x450 - x450ave) - (x250 - x250ave))/ (etax); /* The two BPM positions get subtracted, not added, as they have opposite	dispersion. */
				
				double eV =  eVave + eVdelta;
				
				__st_le64(&bldEbeamInfo.ebeamPhotonEnergy, eV);					
				
			} else {
				bldEbeamInfo.uDamageMask |= __le32(EBEAMPHTONENERGY_DAMAGEMASK);
			}	
			
			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}				

			/* Shantha Condamoor: 7-Jul-2014: shot-to-shot values of X positions of LTU1 BPMS 250 and 450 sent in eBeam BLD data */

			/* BPM LTU1 450 and 250 X positions */ 
			
			if( AVAIL_BMENERGY2X & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamLTU450PosX, (double)bldPulseBlobs[BMENERGY2X].blob->fcbl_bpm_X);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMLTU250POSX_DAMAGEMASK);
			}

			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}						
			
			if( AVAIL_BMENERGY1X & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamLTU250PosX, (double)bldPulseBlobs[BMENERGY1X].blob->fcbl_bpm_X);
			}
			else
			{
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
				bldEbeamInfo.uDamageMask |= __le32(EBEAMLTU450POSX_DAMAGEMASK);
			}

			if ( __ld_le32( &bldEbeamInfo.uDamageMask ) ) {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			} else {
				bldEbeamInfo.uDamage = bldEbeamInfo.uDamage2 = __le32(0);
			}

			epicsMutexUnlock(bldMutex);
		}

		scanIoRequest(bldIoscan);

#ifdef MULTICAST
		/* do MultiCast */
		if(BLD_MCAST_ENABLE)
		{
#ifdef MULTICAST_UDPCOMM
			rtncode = udpCommSend( sFd, &bldEbeamInfo, sizeof(bldEbeamInfo) );
			if ( rtncode < 0 ) {
				if ( BLD_MCAST_DEBUG >= 1 )
					errlogPrintf("Sending multicast packet failed: %s\n", strerror(-rtncode));
			}
#else
			if(-1 == sendto(sFd, (void *)&bldEbeamInfo, sizeof(struct EBEAMINFO), 0, (const struct sockaddr *)&sockaddrDst, sizeof(struct sockaddr_in)))
			{
				if ( BLD_MCAST_DEBUG >= 1 ) perror("Multicast sendto failed\n");
			}
#endif
			bldMcastMsgSent++;
		}
#endif
		this_time = usSinceFiducial();

		if ( this_time > bldMaxPostDelayUs )
			bldMaxPostDelayUs = this_time;
		/* Running average: fn+1 = 127/128 * fn + 1/128 * x = fn - 1/128 fn + 1/128 x */
		bldAvgPostDelayUs += ((int) (- bldAvgPostDelayUs + this_time)) >> 8;

		if ( ! bldBlobSet ) {
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
		}
	}

#ifdef MULTICAST
#ifdef MULTICAST_UDPCOMM
	udpCommClose( sFd );
#else
	close( sFd );
#endif
#endif

	if ( bldBlobSet ) {
		rtncode = fcomFreeBlobSet( bldBlobSet );
		if ( rtncode )
			fprintf(stderr, "Unable to destroy blob set: %s\n", fcomStrerror(rtncode));
		bldBlobSet = 0;
	}

	for ( loop = 0; loop < N_PULSE_BLOBS; loop++ ) {
		rtncode = fcomUnsubscribe( bldPulseID[loop] );
		if ( rtncode )
			fprintf(stderr, "Unable to unsubscribe %s from FCOM: %s\n", bldPulseBlobs[loop].name, fcomStrerror(rtncode));
	}

	/*Should never return from following call*/
	/*SEVCHK(ca_pend_event(0.0),"ca_pend_event");*/
	return(0);
}

void
dumpTsMismatchStats(FILE *f, int reset)
{
	int      i;
	epicsUInt32 cnt [N_PULSE_BLOBS];
	epicsUInt32 cnt1[N_PULSE_BLOBS];
	epicsUInt32 cnt2[N_PULSE_BLOBS];

	if ( bldMutex ) epicsMutexLock( bldMutex );
		memcpy(cnt,  bldUnmatchedTSCount,  sizeof(cnt));
		memcpy(cnt1, bldInvalidAlarmCount, sizeof(cnt1));
		memcpy(cnt2, bldFcomGetErrs,       sizeof(cnt1));
	if ( bldMutex ) epicsMutexUnlock( bldMutex );
	if ( !f )
		f = stdout;
	fprintf(f,"FCOM Data arriving late:\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt[i]);
	}
	fprintf(f,"FCOM Data with bad/invalid status\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt1[i]);
	}
	fprintf(f,"FCOM 'get' message errors\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt2[i]);
	}
	fprintf(f,"MCAST messages posted:     %10lu\n", (unsigned long)bldMcastMsgSent);
	if ( reset ) {
		if ( bldMutex ) epicsMutexLock( bldMutex );
			for ( i=0; i<N_PULSE_BLOBS; i++ ) {
				bldUnmatchedTSCount[i]  = 0;
				bldInvalidAlarmCount[i] = 0;
				bldFcomGetErrs[i]       = 0;
			}
			bldMcastMsgSent = 0;
		if ( bldMutex ) epicsMutexUnlock( bldMutex );
	}
}

#if 1
/**************************************************************************************************/
/* Here we supply the access methods for BSP_timer devBusMappedIO                                  */
/**************************************************************************************************/

static int
timer_delay_rd(DevBusMappedPvt pvt, unsigned *pvalue, dbCommon *prec)
{
	uint64_t us;
	uint64_t clockRate;
#ifdef RTEMS
	/* BSP_timer_clock_get(BSPTIMER) returns the BSP clock rate in hz */
	clockRate = (uint64_t)BSP_timer_clock_get(BSPTIMER);
#else
	clockRate = HiResTicksPerSecond();
#endif

	us = 1000000ULL * (uint64_t)timer_delay_clicks;
	us = us / clockRate;
	*pvalue = (epicsUInt32)us;

	return 0;
}

static int
timer_delay_wr(DevBusMappedPvt pvt, unsigned value, dbCommon *prec)
{
uint64_t clicks;

	/* Convert timer delay in us to ms for fcom polling timeout */
	if ( (timer_delay_ms     = (unsigned)value/1000) < 1 )
		timer_delay_ms = 1;

#ifdef RTEMS
	/* BSP_timer_clock_get(BSPTIMER) returns the BSP clock rate in hz */
	clicks = (uint64_t)BSP_timer_clock_get(BSPTIMER);
#else
	clicks = HiResTicksPerSecond();
#endif
	clicks *= (uint64_t)value;	/* delay from fiducial in us */

	/* timer_delay_clicks is set to timer delay in clock ticks */
	timer_delay_clicks = (uint32_t) (clicks / 1000000ULL);

	return 0;
}

static DevBusMappedAccessRec timer_delay_io = {
	rd: (DevBusMappedRead)  timer_delay_rd,
	wr: (DevBusMappedWrite) timer_delay_wr,
};
#endif

/**************************************************************************************************/
/* Here we supply the driver report function for epics                                            */
/**************************************************************************************************/
static long     BLD_EPICS_Init();
static  long    BLD_EPICS_Report(int level);
static long BLD_report_EBEAMINFO();

static const struct drvet drvBLD = {2,                              /*2 Table Entries */
                              (DRVSUPFUN) BLD_EPICS_Report,  /* Driver Report Routine */
                              (DRVSUPFUN) BLD_EPICS_Init}; /* Driver Initialization Routine */

epicsExportAddress(drvet,drvBLD);

static void
checkDevBusMappedRegister(char *nm, void *ptr)
{
	if ( 0 == devBusMappedRegister(nm, ptr) ) {
		errlogPrintf("WARNING: Unable to register '%s'; related statistics may not work\n", nm);
	}
}

/* implementation */
static long BLD_EPICS_Init()
{
  int rtncode;
  int enable_broadcast = 0;
  int name_len = 100;
  char name[name_len];

  rtncode = gethostname(name, name_len);
  if (rtncode == 0) {
    printf("INFO: BLD hostname is %s\n", name);
    enable_broadcast = 1;
    printf("INFO: *** Enabling Multicast ***\n");
  }
  else {
    printf("ERROR: Unable to get hostname (errno=%d, rtncode=%d)\n", errno, rtncode);
  }


	{
	int loop;
	/* fcomUtilFlag = DP_DEBUG; */
	for ( loop=0; loop < N_PULSE_BLOBS; loop++) {
		printf( "INFO: Looking up fcom ID for %s\n", bldPulseBlobs[loop].name );
		if ( FCOM_ID_NONE == (bldPulseID[loop] = fcomLCLSPV2FcomID(bldPulseBlobs[loop].name)) ) {
			errlogPrintf("FATAL ERROR: Unable to determine FCOM ID for PV %s\n",
				     bldPulseBlobs[loop].name );
			return -1;
		}
		/* printf( "INFO: Subscribing to %s, fcom ID 0x%X\n", bldPulseBlobs[loop].name, bldPulseID[loop] ); */
		rtncode = fcomSubscribe( bldPulseID[loop], FCOM_ASYNC_GET );
		if ( 0 != rtncode ) {
			errlogPrintf("FATAL ERROR: Unable to subscribe %s (0x%08"PRIx32") to FCOM: %s\n",
					bldPulseBlobs[loop].name,
					bldPulseID[loop],
					fcomStrerror(rtncode));
			return -1;
		}
		printf( "INFO: Subscribed  to %s, fcom ID 0x%X\n", bldPulseBlobs[loop].name, (unsigned int) bldPulseID[loop] );
	}
	/* fcomUtilFlag = DP_ERROR; */
	}

	/* Allocate blob set here so that the flag that is read
	 * into a record already contains the final value, ready
	 * for being picked up by PINI
	 */
	if ( epicsThreadSleepQuantum() > 0.004 )
	{
		errlogPrintf(	"WARNING: system clock rate not high enough to timeout! May need to use asynchronous mode\n");
		errlogPrintf(	"sysconf clock ticks/sec = %ld, epicsThreadSleepQuantum = %0.3e\n",
						sysconf( _SC_CLK_TCK ), epicsThreadSleepQuantum() );
	}
#if 0
	else
#endif
	{
		if ( (rtncode = fcomAllocBlobSet( bldPulseID, sizeof(bldPulseID)/sizeof(bldPulseID[0]), &bldBlobSet)) ) {
			errlogPrintf("ERROR: Unable to allocate blob set: %s; trying asynchronous mode\n", fcomStrerror(rtncode));
			bldBlobSet = 0;
			
		} else
		{
			bldUseFcomSet = 1;			
		}
	}

	if ( devBusMappedRegisterIO("bld_timer_io", &timer_delay_io) )
		errlogPrintf("ERROR: Unable to register I/O methods for timer delay\n"
                     "SOFTWARE MAY NOT WORK PROPERLY\n");

	/* These are simple local memory devBus devices for showing fcom status in PV's */
	checkDevBusMappedRegister("bld_stat_bad_t", &bldUnmatchedTSCount);
	checkDevBusMappedRegister("bld_stat_bad_d", &bldInvalidAlarmCount);
	checkDevBusMappedRegister("bld_stat_bad_f", &bldFcomGetErrs);
	checkDevBusMappedRegister("bld_stat_msg_s", &bldMcastMsgSent);
	checkDevBusMappedRegister("bld_stat_min_f", &bldMinFcomDelayUs);
	checkDevBusMappedRegister("bld_stat_max_f", &bldMaxFcomDelayUs);
	checkDevBusMappedRegister("bld_stat_avg_f", &bldAvgFcomDelayUs);
	checkDevBusMappedRegister("bld_stat_max_p", &bldMaxPostDelayUs);
	checkDevBusMappedRegister("bld_stat_avg_p", &bldAvgPostDelayUs);
	checkDevBusMappedRegister("bld_fcom_use_s", &bldUseFcomSet);

	scanIoInit(&bldIoscan);

	bldMutex = epicsMutexMustCreate();

	if (enable_broadcast) {
	  BLDMCastStart( enable_broadcast, getenv("IPADDR1") );
	}
	else {
	  errlogPrintf("WARN: *** Not starting BLDMCast task ***\n");
	}

	return 0;
}

static long BLD_EPICS_Report(int level)
{
    printf("\n"BLD_DRV_VERSION"\n\n");
	printf("Compiled with options:\n");

	printf("USE_PULSE_CA     :   UNDEFINED (use FCOM not CA to obtain pulse-by-pulse data)\n");

	printf("USE_CA_ADD_EVENT :");
#ifdef  USE_CA_ADD_EVENT
		printf(             "  DEFINED (use ca_add_array_event, not ca_create_subscription)\n");
#else
		printf(             "  UNDEFINED (use ca_create_subscription, not ca_add_array_event)\n");
#endif

	printf("MULTICAST        :");
#ifdef MULTICAST
		printf(             "  DEFINED (use multicast interface to send data)\n");
#ifdef MULTICAST_UDPCOMM
	printf("MULTICAST_UDPCOMM:");
			printf(         "  DEFINED (use UDPCOMM/lanIpBasic, not BSD sockets for sending BLD multicast messages)\n");
#else
			printf(         "  DEFINED (use BSD sockets, not UDPCOMM/lanIpBasic for sending BLD multicast messages)\n");
#endif
#else
		printf(             "  UNDEFINED (use of multicast interface to send data DISABLED)\n");
#endif
		
	printf("FCOM             :");
	if ( bldBlobSet )
		printf("  Uses a blob set to receive data synchronously\n");
	else
		printf("  Delays (using a hardware timer) and reads data asynchronously\n");

	printf("Latest PULSEID   : %d\n", __ld_le32(&bldEbeamInfo.uFiducialId));

	if ( level > 3 ) {
	  BLD_report_EBEAMINFO();
	}

	/* scondam: 19-Jun-2014: BLDSender and BLDReceiver apps have been split.
	   bld_receivers_report() not needed for Sender. */
	   
	/* bld_receivers_report(level); */

    return 0;
}

static long BLD_report_EBEAMINFO()
{
  printf("ts_sec: %d\n", __ld_le32(&bldEbeamInfo.ts_sec));
  printf("ts_nsec: %d\n", __ld_le32(&bldEbeamInfo.ts_nsec));
  printf("mMBZ1: %d\n", __ld_le32(&bldEbeamInfo.uMBZ1));
  printf("uFiducialId: %d\n", __ld_le32(&bldEbeamInfo.uFiducialId));
  printf("mMBZ2: %d\n", __ld_le32(&bldEbeamInfo.uMBZ2));

  /* Xtc Section 1 */
  printf("uDamage: %d\n", __ld_le32(&bldEbeamInfo.uDamage));
  printf("uLogicalId: %d\n", __ld_le32(&bldEbeamInfo.uLogicalId));
  printf("uPhysicalId: %d\n", __ld_le32(&bldEbeamInfo.uPhysicalId));
  printf("uDataType: %d\n", __ld_le32(&bldEbeamInfo.uDataType));
  printf("uExtentSize: %d\n", __ld_le32(&bldEbeamInfo.uExtentSize));

  /* Xtc Section 2 */
  printf("uDamage2: %d\n", __ld_le32(&bldEbeamInfo.uDamage2));
  printf("uLogicalId2: %d\n", __ld_le32(&bldEbeamInfo.uLogicalId2));
  printf("uPhysicalId2: %d\n", __ld_le32(&bldEbeamInfo.uPhysicalId2));
  printf("uDataType2: %d\n", __ld_le32(&bldEbeamInfo.uDataType2));
  printf("uExtentSize2: %d\n", __ld_le32(&bldEbeamInfo.uExtentSize2));

  /* Data */
  printf("uDamageMask: %d\n", __ld_le32(&bldEbeamInfo.uDamageMask));
  printf("ebeamCharge: %f\n", __ld_le64(&bldEbeamInfo.ebeamCharge));
  printf("ebeamL3Energy: %f\n", __ld_le64(&bldEbeamInfo.ebeamL3Energy));
  printf("ebeamLTUPosX: %f\n", __ld_le64(&bldEbeamInfo.ebeamLTUPosX));
  printf("ebeamLTUPosY: %f\n", __ld_le64(&bldEbeamInfo.ebeamLTUPosY));
  printf("ebeamLTUAngX: %f\n", __ld_le64(&bldEbeamInfo.ebeamLTUAngX));
  printf("ebeamLTUAngY: %f\n", __ld_le64(&bldEbeamInfo.ebeamLTUAngY));
  
  printf("ebeamBC1Current: %f Amps\n", __ld_le64(&bldEbeamInfo.ebeamBC1Current));
  printf("ebeamBC1Energy: %f mm\n", __ld_le64(&bldEbeamInfo.ebeamBC1Energy));
  printf("ebeamBC2Current: %f Amps\n", __ld_le64(&bldEbeamInfo.ebeamBC2Current));
  printf("ebeamBC2Energy: %f mm\n", __ld_le64(&bldEbeamInfo.ebeamBC2Energy));

  printf("ebeamUndPosX: %f mm\n", __ld_le64(&bldEbeamInfo.ebeamUndPosX));
  printf("ebeamUndAngX: %f mrad\n", __ld_le64(&bldEbeamInfo.ebeamUndAngX));
  printf("ebeamUndPosY: %f mm\n", __ld_le64(&bldEbeamInfo.ebeamUndPosY));
  printf("ebeamUndAngY: %f mrad\n", __ld_le64(&bldEbeamInfo.ebeamUndAngY));

  printf("ebeamXTCAVAmpl: %f MeV\n", __ld_le64(&bldEbeamInfo.ebeamXTCAVAmpl));
  printf("ebeamXTCAVPhase: %f deg\n", __ld_le64(&bldEbeamInfo.ebeamXTCAVPhase));
  printf("ebeamDMP502Charge: %f Nel\n", __ld_le64(&bldEbeamInfo.ebeamDMP502Charge));
  
  printf("ebeamPhotonEnergy: %f eV\n", __ld_le64(&bldEbeamInfo.ebeamPhotonEnergy));
  printf("ebeamLTU450PosX: %f deg\n", __ld_le64(&bldEbeamInfo.ebeamLTU450PosX));
  printf("ebeamLTU250PosX: %f Nel\n", __ld_le64(&bldEbeamInfo.ebeamLTU250PosX));  

  printf("Data Available Mask: %x\n", dataAvailable);

  return 0;
}

