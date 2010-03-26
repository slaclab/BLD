/* $Id: BLDMCast.c,v 1.29 2010/03/25 15:16:54 strauman Exp $ */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#include <ppu_intrinsics.h>

#include <bsp/gt_timer.h> /* This is MVME6100/5500 only */

#include "BLDMCast.h"

#define BLD_DRV_VERSION "BLD driver $Revision: 1.29 $/$Name:  $"

#define CA_PRIORITY	CA_PRIORITY_MAX		/* Highest CA priority */

#define TASK_PRIORITY	epicsThreadPriorityMax	/* Highest EPICS thread priority */

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
    BMPOSITION1Y,
    BMPOSITION2Y,
    BMPOSITION3Y,
    BMPOSITION4Y,
    BMBUNCHLEN
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


static BLDPV staticPVs[]=
{
    [DSPR1]  = {"BLD:SYS0:500:DSPR1",  1, AVAIL_DSPR1,  NULL, NULL},	/* For Energy */
    [DSPR2]  = {"BLD:SYS0:500:DSPR2",  1, AVAIL_DSPR2,  NULL, NULL},	/* For Energy */
    [E0BDES] = {"BEND:LTU0:125:BDES",  1, AVAIL_E0BDES, NULL, NULL},	/* Energy in MeV */
    [FMTRX]  = {"BLD:SYS0:500:FMTRX", 32, AVAIL_FMTRX,  NULL, NULL}	/* For Position */
};
#define N_STATIC_PVS (sizeof(staticPVs)/sizeof(struct BLDPV))

static BLDPV pulsePVs[]=
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
    [BMPOSITION1Y] = {"BPMS:LTU1:720:Y", 1, AVAIL_BMPOSITION1Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION2Y] = {"BPMS:LTU1:730:Y", 1, AVAIL_BMPOSITION2Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION3Y] = {"BPMS:LTU1:740:Y", 1, AVAIL_BMPOSITION3Y, NULL, NULL},	/* Position in mm/mrad */
    [BMPOSITION4Y] = {"BPMS:LTU1:750:Y", 1, AVAIL_BMPOSITION4Y, NULL, NULL},	/* Position in mm/mrad */

    [BMBUNCHLEN]   = {"BLEN:LI24:886:BIMAX", 1, AVAIL_BMBUNCHLEN, NULL, NULL},	/* Bunch Length in Amps */
};
#define N_PULSE_PVS (sizeof(pulsePVs)/sizeof(struct BLDPV))

enum PVAVAILMASK dataAvailable = 0;

#if 0
static EBEAMINFO ebeamInfoPreFill =
{
    0,0,/* to be changed pulse by pulse */
    0,
    0,	/* to be changed pulse by pulse, low 17 bits of timestamp nSec */
    0,

    0, /* to be changed, if error, 0x4000 */
    0x06000000,
    0,
    0x1000f,
    80,

    0, /* to be changed, if error, 0x4000 */
    0x06000000,
    0,
    0x1000f,
    80,

    0, /* to be changed, mask */

    {d:0.0},
    {d:0.0},
    {d:0.0},
    {d:0.0},
    {d:0.0},
    {d:0.0},

    {d:0.0}
};
#else
#warning "remove eventually"
#endif

#define MCAST_LOCAL_IP	"172.27.225.21"
#define MCAST_DST_IP	(inet_addr("239.255.24.0"))
#define MCAST_DST_PORT	(htons(10148))
#define MCAST_TTL	8

#define FETCH_PULSE_PVS	/* Otherwise we monitor pulse PVs */
#define USE_CA_ADD_EVENT	/* ca_create_subscription seems buggy */
/* T.S: I doubt that ca_create_subscription works any different. 
 * ca_add_array_event is just a macro mapping to the
 * ca_add_masked_array_event() wrapper which calls
 * ca_create_subscription with the DBE_VALUE | DBE_ALARM mask ...
 */
#define MULTICAST	/* Use multicast interface */

int BLD_MCAST_ENABLE = 1;
epicsExportAddress(int, BLD_MCAST_ENABLE);

int BLD_MCAST_DEBUG = 0;
epicsExportAddress(int, BLD_MCAST_DEBUG);

int DELAY_FROM_FIDUCIAL = 5000;	/* in microseconds */
epicsExportAddress(int, DELAY_FROM_FIDUCIAL);

int DELAY_FOR_CA = 60;	/* in seconds */
epicsExportAddress(int, DELAY_FOR_CA);

/* Share with device support */
EBEAMINFO ebeamInfo;
volatile int bldAllPVsConnected = FALSE;
int bldInvalidAlarmCount = 0;
int bldUnmatchedTSCount = 0;
IOSCANPVT  ioscan;         /* Trigger EPICS record */
epicsMutexId mutexLock = NULL;	/* Protect staticPVs' values */
/* Share with device support */


/*==========================================================*/
static in_addr_t mcastIntfIp = 0;


static void evr_timer_isr(void *arg);
static int BLDMCastTask(void * parg);
static void BLDMCastTaskEnd(void * parg);

static epicsEventId EVRFireEvent = NULL;

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

    scanIoInit(&ioscan);

    mutexLock = epicsMutexMustCreate();

    /******************************************************************* Setup high resolution timer ***************************************************************/
    /* you need to setup the timer only once (to connect ISR) */
    BSP_timer_setup( 0 /* use first timer */, evr_timer_isr, 0, 0 /* do not reload timer when it expires */);

    epicsAtExit(BLDMCastTaskEnd, NULL);

    return (int)(epicsThreadMustCreate("BLDMCast", TASK_PRIORITY, 20480, (EPICSTHREADFUNC)BLDMCastTask, NULL));
}

void EVRFire(void *unused)
{/* This funciton will be registered with EVR callback */
    epicsUInt32 rate_mask = MOD5_30HZ_MASK|MOD5_10HZ_MASK|MOD5_5HZ_MASK|MOD5_1HZ_MASK|MOD5_HALFHZ_MASK;  /* can be 30HZ,10HZ,5HZ,1HZ,HALFHZ */

    /* get the current pattern data - check for good status */
    evrModifier_ta modifier_a;
    epicsTimeStamp time_s;
    unsigned long  patternStatus; /* see evrPattern.h for values */
    int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
    /* This is 120Hz. So printf will screw timing */
    if(BLD_MCAST_DEBUG >= 4) errlogPrintf("EVR fires\n");
    if (!status)
    {/* check for LCLS beam and rate-limiting */
        if ((modifier_a[4] & MOD5_BEAMFULL_MASK) && (modifier_a[4] & rate_mask))
	{/* ... do beam-sync rate-limited processing here ... */
	 /* call 'BSP_timer_start()' to set/arm the hardware */
            uint64_t delayFromFiducial = 0;

            /* This is 30Hz. So printf might screw timing */
            if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Timer Starts\n");
            delayFromFiducial = BSP_timer_clock_get(0) * DELAY_FROM_FIDUCIAL;	/* delay from fiducial in us */
	    BSP_timer_start( 0, (uint32_t) (delayFromFiducial / 1000000) );
	}
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

    epicsMutexLock(mutexLock);
    if (args.status == ECA_NORMAL)
    {
        memcpy(pPV->pTD, args.dbr, dbr_size_n(args.type, args.count));
        if(pPV->pTD->severity >= INVALID_ALARM)	/* We don't care timestamp for staticPVs. As long as it is not invalid, they are ok */
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
    epicsMutexUnlock(mutexLock);
}

static void BLDMCastTaskEnd(void * parg)
{
    int loop;

    bldAllPVsConnected = FALSE;
    epicsThreadSleep(2.0);

    epicsMutexLock(mutexLock);
    for(loop=0; loop<N_STATIC_PVS; loop++)
    {
        if(staticPVs[loop].pvChId)
        {
            ca_clear_channel(staticPVs[loop].pvChId);
            staticPVs[loop].pvChId = NULL;
        }
        if(staticPVs[loop].pTD)
        {
            free(staticPVs[loop].pTD);
            staticPVs[loop].pTD = NULL;
        }
    }

    for(loop=0; loop<N_PULSE_PVS; loop++)
    {
        if(pulsePVs[loop].pvChId)
        {
            ca_clear_channel(pulsePVs[loop].pvChId);
            pulsePVs[loop].pvChId = NULL;
        }
        if(pulsePVs[loop].pTD)
        {
            free(pulsePVs[loop].pTD);
            pulsePVs[loop].pTD = NULL;
        }
    }
    epicsMutexUnlock(mutexLock);
    printf("BLDMCastTaskEnd\n");

    return;
}

static int BLDMCastTask(void * parg)
{
    int		loop;
    int		rtncode;

    int		sFd;
    struct sockaddr_in sockaddrSrc;
    struct sockaddr_in sockaddrDst;
    unsigned char mcastTTL;

    /************************************************************************* Prepare MultiCast *******************************************************************/
#ifdef MULTICAST
    sFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sFd == -1)
    {
        errlogPrintf("Failed to create socket for multicast\n");
	epicsMutexDestroy(mutexLock);
        return -1;
    }

    /* no need to enlarge buffer since we have small packet, system default should be big enought */

    memset(&sockaddrSrc, 0, sizeof(struct sockaddr_in));
    memset(&sockaddrDst, 0, sizeof(struct sockaddr_in));

    sockaddrSrc.sin_family      = AF_INET;
    sockaddrSrc.sin_addr.s_addr = INADDR_ANY;
    sockaddrSrc.sin_port        = 0;

    sockaddrDst.sin_family      = AF_INET;
    sockaddrDst.sin_addr.s_addr = MCAST_DST_IP;
    sockaddrDst.sin_port        = MCAST_DST_PORT;

    if( bind( sFd, (struct sockaddr *) &sockaddrSrc, sizeof(struct sockaddr_in) ) == -1 )
    {
        errlogPrintf("Failed to bind local socket for multicast\n");
	close(sFd);
	epicsMutexDestroy(mutexLock);
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
	epicsMutexDestroy(mutexLock);
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
	    epicsMutexDestroy(mutexLock);
            return -1;
        }
    }
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
	SEVCHK(ca_create_channel(staticPVs[loop].name, NULL/*connectionCallback*/, NULL/*&(staticPVs[loop])*/, CA_PRIORITY ,&(staticPVs[loop].pvChId)), "ca_create_channel");
        /* Not very necessary */
	/* SEVCHK(ca_replace_access_rights_event(staticPVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event"); */

        /* We could do subscription in connetion callback. But in this case, better to enforce all connection */
        rtncode = ca_pend_io(15.0);
        if (rtncode == ECA_TIMEOUT)
        {
            errlogPrintf("Channel connect timed out: '%s' not found.\n", staticPVs[loop].name);
            return -1;
        }
        if(staticPVs[loop].nElems != ca_element_count(staticPVs[loop].pvChId))
        {
            errlogPrintf("Number of elements [%ld] of '%s' does not match expectation.\n", ca_element_count(staticPVs[loop].pvChId), staticPVs[loop].name);
            return -1;
        }

        if(DBF_DOUBLE != ca_field_type(staticPVs[loop].pvChId))
        {/* Native data type has to be double */
            errlogPrintf("Native data type of '%s' is not double.\n", staticPVs[loop].name);
            return -1;
        }

        /* Everything should be double, even not, do conversion */
        staticPVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, staticPVs[loop].nElems), "callocMustSucceed");
#ifdef USE_CA_ADD_EVENT
        SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, staticPVs[loop].nElems, staticPVs[loop].pvChId, eventCallback, &(staticPVs[loop]), 0.0, 0.0, 0.0, NULL), "ca_add_array_event");
#else
        SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, staticPVs[loop].nElems, staticPVs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(staticPVs[loop]), NULL), "ca_create_subscription");
#endif
    }

    ca_flush_io();
    /* ca_pend_event(2.0); */

    /* We only fetch pulse PVs */
    for(loop=0; loop<N_PULSE_PVS; loop++)
    {
        rtncode = ECA_NORMAL;
        /* No need for connectionCallback since we want to wait till connected */
	SEVCHK(ca_create_channel(pulsePVs[loop].name, NULL/*connectionCallback*/, NULL/*&(pulsePVs[loop])*/, CA_PRIORITY ,&(pulsePVs[loop].pvChId)), "ca_create_channel");
        /* Not very necessary */
	/* SEVCHK(ca_replace_access_rights_event(pulsePVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event"); */

        /* We could do subscription in connetion callback. But in this case, better to enforce all connection */
        rtncode = ca_pend_io(15.0);
        if (rtncode == ECA_TIMEOUT)
        {
            errlogPrintf("Channel connect timed out: '%s' not found.\n", pulsePVs[loop].name);
            return -1;
        }
        if(pulsePVs[loop].nElems != ca_element_count(pulsePVs[loop].pvChId))
        {
            errlogPrintf("Number of elements of '%s' does not match expectation.\n", pulsePVs[loop].name);
            return -1;
        }

        if(DBF_DOUBLE != ca_field_type(pulsePVs[loop].pvChId))
        {/* native type has to be double */
            errlogPrintf("Native data type of '%s' is not double.\n", pulsePVs[loop].name);
            return -1;
        }

        /* Everything should be double, even not, do conversion */
        pulsePVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, pulsePVs[loop].nElems), "callocMustSucceed");
#ifdef FETCH_PULSE_PVS
        /* We don't subscribe to pulse PVs */
#else
#ifdef USE_CA_ADD_EVENT
        SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, pulsePVs[loop].nElems, pulsePVs[loop].pvChId, eventCallback, &(pulsePVs[loop]), 0.0, 0.0, 0.0, NULL), "ca_add_array_event");
#else
        SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, pulsePVs[loop].nElems, pulsePVs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(pulsePVs[loop]), NULL), "ca_create_subscription");
#endif
#endif
    }
    ca_flush_io();


    /* All ready to go, create event and register with EVR */
    EVRFireEvent = epicsEventMustCreate(epicsEventEmpty);
    /* Register EVRFire */
    evrTimeRegister(EVRFire, 0);

    bldAllPVsConnected = TRUE;
    printf("All PVs are successfully connected!\n");

	/* Prefill ebeamInfo with constant values */
	ebeamInfo.uMBZ1       = __le32(0);
	ebeamInfo.uMBZ2       = __le32(0);

	ebeamInfo.uLogicalId  = __le32(0x06000000);
	ebeamInfo.uPhysicalId = __le32(0);
	ebeamInfo.uDataType   = __le32(0x1000f);
	ebeamInfo.uExtentSize = __le32(80);

	ebeamInfo.uLogicalId2 = __le32(0x06000000);
	ebeamInfo.uPhysicalId2= __le32(0);
	ebeamInfo.uDataType2  = __le32(0x1000f);
	ebeamInfo.uExtentSize2= __le32(80);

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

#ifdef FETCH_PULSE_PVS
        /* Timer fires ok, let's then get pulse PVs */
        /* This is 30Hz. So printf might screw timing */
        if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Do work!\n");
        rtncode = ECA_NORMAL;
        for(loop=0; loop<N_PULSE_PVS; loop++)
        {
            rtncode = ca_array_get(DBR_TIME_DOUBLE, pulsePVs[loop].nElems, pulsePVs[loop].pvChId, (void *)(pulsePVs[loop].pTD));
        }
        rtncode = ca_pend_io(DEFAULT_CA_TIMEOUT);
        if (rtncode != ECA_NORMAL)
        {
            if(BLD_MCAST_DEBUG) errlogPrintf("Something wrong when fetch pulse-by-pulse PVs.\n");
			epicsMutexLock(mutexLock);
            ebeamInfo.uDamageMask = __le32(0xffffffff); /* no information available */
            ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
			epicsMutexUnlock(mutexLock);
        }
        else
#endif
        {/* Got all PVs, do calculation including checking severity and timestamp */
			epicsTimeStamp *p_refTime;

            /* Assume the first timestamp is right */
			p_refTime = &pulsePVs[BMCHARGE].pTD->stamp;

            epicsMutexLock(mutexLock);

			__st_le32(&ebeamInfo.ts_sec,      p_refTime->secPastEpoch);
			__st_le32(&ebeamInfo.ts_nsec,     p_refTime->nsec);
            __st_le32(&ebeamInfo.uFiducialId, PULSEID((*p_refTime)));


            for(loop=0; loop<N_PULSE_PVS; loop++)
            {
                dataAvailable |= pulsePVs[loop].availMask;

                if(pulsePVs[loop].pTD->severity >= INVALID_ALARM )
                {
                    dataAvailable &= ~ pulsePVs[loop].availMask;
                    bldInvalidAlarmCount++;
                    if(BLD_MCAST_DEBUG) errlogPrintf("%s has severity %d\n", pulsePVs[loop].name, pulsePVs[loop].pTD->severity);
                }

                if( 0 != memcmp(p_refTime, &(pulsePVs[loop].pTD->stamp), sizeof(epicsTimeStamp)) )
                {
                    dataAvailable &= ~ pulsePVs[loop].availMask;
                    bldUnmatchedTSCount++;
                    if(BLD_MCAST_DEBUG) errlogPrintf("%s has unmatched timestamp.nsec [0x%08X,0x%08X]\n",
                                         pulsePVs[loop].name, p_refTime->nsec, pulsePVs[loop].pTD->stamp.nsec);
                }
            }

            ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(0);
            ebeamInfo.uDamageMask = __le32(0);

            /* Calculate beam charge */
            if( (dataAvailable & AVAIL_BMCHARGE) )
            {
                __st_le64(&ebeamInfo.ebeamCharge, pulsePVs[BMCHARGE].pTD->value * 1.602e-10);
            }
            else
            {
                ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
                ebeamInfo.uDamageMask |= __le32(0x1);
            }

#define AVAIL_L3ENERGY (AVAIL_BMENERGY1X | AVAIL_BMENERGY2X | AVAIL_DSPR1 | AVAIL_DSPR2 | AVAIL_E0BDES)

            /* Calculate beam energy */
            if( AVAIL_L3ENERGY == (AVAIL_L3ENERGY & dataAvailable ) )
            {
                double tempD;
                tempD = pulsePVs[BMENERGY1X].pTD->value/(1000.0 * staticPVs[DSPR1].pTD->value);
                tempD += pulsePVs[BMENERGY2X].pTD->value/(1000.0 * staticPVs[DSPR2].pTD->value);
                tempD = tempD/2.0 + 1.0;
                tempD *= staticPVs[E0BDES].pTD->value * 1000.0;
                __st_le64(&ebeamInfo.ebeamL3Energy, tempD);
            }
            else
            {
                ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
                ebeamInfo.uDamageMask |= __le32(0x2);
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

                pMatrixValue = &(staticPVs[FMTRX].pTD->value);
                for(i=0;i<4;i++)
                {
                    tempDA[i] = 0.0;
                    for(j=0;j<8;j++)
                        tempDA[i] += pMatrixValue[i*8+j] * pulsePVs[BMPOSITION1X+j].pTD->value;
                }
                __st_le64(&ebeamInfo.ebeamLTUPosX, tempDA[0]);
                __st_le64(&ebeamInfo.ebeamLTUPosY, tempDA[1]);
                __st_le64(&ebeamInfo.ebeamLTUAngX, tempDA[2]);
                __st_le64(&ebeamInfo.ebeamLTUAngY, tempDA[3]);
            }
            else
            {
                ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
                ebeamInfo.uDamageMask |= __le32(0x3C);
            }

            /* Copy bunch length */
            if( (AVAIL_BMBUNCHLEN & dataAvailable) )
            {
                __st_le64(&ebeamInfo.ebeamBunchLen, pulsePVs[BMBUNCHLEN].pTD->value);
            }
            else
            {
                ebeamInfo.uDamage = ebeamInfo.uDamage2 = __le32(EBEAM_INFO_ERROR);
                ebeamInfo.uDamageMask |= __le32(0x40);
            }

            epicsMutexUnlock(mutexLock);

       }

       scanIoRequest(ioscan);

#ifdef MULTICAST
       /* do MultiCast */
       if(BLD_MCAST_ENABLE)
       {
	   if(-1 == sendto(sFd, (void *)&ebeamInfo, sizeof(struct EBEAMINFO), 0, (const struct sockaddr *)&sockaddrDst, sizeof(struct sockaddr_in)))
	   {
                if(BLD_MCAST_DEBUG) perror("Multicast sendto failed\n");
	   }
       }
#endif
    }

    /*Should never return from following call*/
    /*SEVCHK(ca_pend_event(0.0),"ca_pend_event");*/
    return(0);
}

/**************************************************************************************************/
/* Here we supply the driver report function for epics                                            */
/**************************************************************************************************/
static long     BLD_EPICS_Init();
static  long    BLD_EPICS_Report(int level);

const struct drvet drvBLD = {2,                              /*2 Table Entries */
                              (DRVSUPFUN) BLD_EPICS_Report,  /* Driver Report Routine */
                              (DRVSUPFUN) BLD_EPICS_Init}; /* Driver Initialization Routine */

epicsExportAddress(drvet,drvBLD);

/* implementation */
static long BLD_EPICS_Init()
{
    BLDMCastStart(1, getenv("IPADDR1"));
    return 0;
}

static long BLD_EPICS_Report(int level)
{
    printf("\n"BLD_DRV_VERSION"\n\n");

    return 0;
}


