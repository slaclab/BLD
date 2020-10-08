/* $Id: BLDMCast.c,v 1.76 2016/06/19 00:07:58 bhill Exp $ */
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
		28-Feb-2014 - L.Piccoli - BLD-R2-4-0 - Merged BLD-R2-0-0-BR branch with MAIN_TRUNK. Addition of TCAV/DMPH PVs to BLD. Version 0x5000f
		12-Mar-2014 - L.Piccoli - BLD-R2-5-4, BLD-R2-5-3, BLD-R2-5-2, BLD-R2-5-1, BLD-R2-5-0, BLD-R2-4-6 - Prevents BLDCast task on all iocs except ioc-sys0-bd01
		20-Jun-2014 - S.Condamoor - BLD-R2-5-5 - BLDSender and BLDReceiver apps have been split. bld_receivers_report() not needed for Sender
		7-Jul-2014  - S.Condamoor - BLD-R2-6-0 - Added Photon Energy Calculation to eBeam BLD MCAST data . Version 0x6000f
												Added code to set the 0x20000 damage bit if the EPICS variables become disconnected, or
												    if the BPM data is unavailable.
		11-Nov-2014: S.Condamoor - BLD-R2-6-2 L.Piccoli moved Und Launch Feeback to FB05:RF05. FBCK:FB03:TR05:STATES changed to FBCK:FB05:TR05:STATES
		2-Feb-2015  - S.Condamoor - BLD-R2-6-3 - Fix for etax in Photon Energy Calculation per PCDS request. Version 0x7000f
		18-Sep-2015 - B.Hill - Modified for linux compatibility
		16-Oct-2015 - B.Hill - Fixed some timing issues, added more fcom stats, and tossed stale code
-----------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#ifdef RTEMS
#include <lanIpBasic.h>
#endif

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
#include <initHooks.h>
#include <cadef.h>
#include <drvSup.h>
#include <alarm.h>
#include <cantProceed.h>
#include <errlog.h>
#include <epicsExit.h>
#include <iocsh.h>

#include "HiResTime.h"
#include "ContextTimer.h"
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

#define BLD_DRV_VERSION "BLD driver $Revision: 1.76 $/$Name:  $"

#define CA_PRIORITY     CA_PRIORITY_MAX         /* Highest CA priority */

#define TASK_PRIORITY   epicsThreadPriorityMax  /* Highest EPICS thread priority */

#define DEFAULT_CA_TIMEOUT	0.04		/* Default CA timeout, for 30Hz */
#define DEFAULT_EVR_TIMEOUT	0.2		/* Default EVR event timeout, for 30Hz */

#define MCAST_TTL	8

#define MULTICAST           /* Use multicast interface */
#ifdef RTEMS
#define MULTICAST_UDPCOMM   /* Use UDPCOMM for data output; BSD sockets otherwise */
#else
#define MULTICAST_UDPCOMM   /* Use UDPCOMM for data output; BSD sockets otherwise */
#endif

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
/* Doubles as enumerated names for each blob */
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
    DMP_CHARGE,
    XTCAV_AMP,
	/* Define Y after everything else so that fcom array doesn't have to use them */
    BMPOSITION1Y,
    BMPOSITION2Y,
    BMPOSITION3Y,
    BMPOSITION4Y
};/* the definition here must match the PV definition below, the order is critical as well */

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
#define AVAIL_DMP_CHARGE   0x100000
#define AVAIL_XTCAV_AMP    0x200000

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

  chid		             caChId;

  struct dbr_time_double * pTD;
  unsigned int			subscribed;
  int					status;

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

#if defined(BLD_SXR)
BLDPV bldStaticPVs[]=
{
    [DSPR1]    = { "BLD:SYS0:600:DSPR1",   1,  AVAIL_DSPR1,    NULL, NULL },	/* For Energy */
    [DSPR2]    = { "BLD:SYS0:600:DSPR2",   1,  AVAIL_DSPR2,    NULL, NULL },	/* For Energy */
    [E0BDES]   = { "BEND:LTUS:525:BDES",   1,  AVAIL_E0BDES,   NULL, NULL },	/* Energy in MeV */
    [FMTRX]    = { "BLD:SYS0:600:FMTRX",   32, AVAIL_FMTRX,    NULL, NULL },	/* For Position */
    // TODO (rreno): Get the SXR varaints of the following PVs from Physics
    [PHOTONEV] = { "SIOC:SYS0:ML00:AO628", 1,  AVAIL_PHOTONEV, NULL, NULL },    /* For shot-to-shot Photon Energy */
    [X450AVE]  = { "SIOC:SYS0:ML02:AO170", 1,  AVAIL_X450AVE,  NULL, NULL },	/* Average of last few hundred data points of X POS in LTU BPM x450 */
    [X250AVE]  = { "SIOC:SYS0:ML02:AO171", 1,  AVAIL_X250AVE,  NULL, NULL },	/* Average of last few hundred data points of X POS in LTU BPM x250 */
    [7]        = { "EVR:B34:EVR05:LINK",   1,  0,              NULL, NULL },	/* Test only */
};
#else
BLDPV bldStaticPVs[]=
{
    [DSPR1]    = { "BLD:SYS0:500:DSPR1",   1,  AVAIL_DSPR1,    NULL, NULL },	/* For Energy */
    [DSPR2]    = { "BLD:SYS0:500:DSPR2",   1,  AVAIL_DSPR2,    NULL, NULL },	/* For Energy */
    [E0BDES]   = { "BEND:LTUH:125:BDES",   1,  AVAIL_E0BDES,   NULL, NULL },	/* Energy in MeV */
    [FMTRX]    = { "BLD:SYS0:500:FMTRX",   32, AVAIL_FMTRX,    NULL, NULL },	/* For Position */
/* Shantha Condamoor: 7-Jul-2014: The following are for matlab PVs that are used in shot-to-shot photon energy calculations*/
    [PHOTONEV] = { "SIOC:SYS0:ML00:AO627", 1,  AVAIL_PHOTONEV, NULL, NULL },    /* For shot-to-shot Photon Energy */
    [X450AVE]  = { "SIOC:SYS0:ML02:AO041", 1,  AVAIL_X450AVE,  NULL, NULL },	/* Average of last few hundred data points of X POS in LTU BPM x450 */
    [X250AVE]  = { "SIOC:SYS0:ML02:AO040", 1,  AVAIL_X250AVE,  NULL, NULL },	/* Average of last few hundred data points of X POS in LTU BPM x250 */
    [7]        = { "EVR:B34:EVR05:LINK",   1,  0,              NULL, NULL },	/* Test only */
};
#endif // BLD_SXR

#define N_STATIC_PVS (sizeof(bldStaticPVs)/sizeof(bldStaticPVs[0]))

/*
 * Note: The BLOB name sequence below must match the fcom_stats.substitution file
 * and the above enumeration for PULSEPVSINDEX
 */
#if defined(BLD_SXR)
BLDBLOB bldPulseBlobs[] =
{
    /**
    * Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
    */
    /* BMCHARGE: BlobSet mask bit 0x0001 */
    [BMCHARGE] = { name: "BPMS:IN20:221:TMIT", blob: 0, aMsk: AVAIL_BMCHARGE },     /* Charge in Nel, 1.602e-10 nC per Nel*/

    /**
    * Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
    * where E0 is the final desired energy at the LTU (the magnet setting BEND:LTUS:525:BDES*1000)
    * dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
    * BPM1x = [BPMS:LTUH:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
    * BPM2x = [BPMS:LTUH:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
    */
    /* BMENERGY1X: BlobSet mask bit 0x0002 */
    // TODO (rreno): Get these energy PVs from Physics
    [BMENERGY1X] = { name: "BPMS:LTUS:180:X", blob: 0, aMsk: AVAIL_BMENERGY1X },    /* Actually X pos in mm */
    /* BMENERGY2X: BlobSet mask bit 0x0004 */
    [BMENERGY2X] = { name: "BPMS:LTUS:450:X", blob: 0, aMsk: AVAIL_BMENERGY2X },    /* Actually X pos in mm */

    /**
    * Position X, Y, Angle X, Y at LTU:
    * Using the LTU Feedback BPMs: BPMS:LTUS:660,680,740,750
    * The best estimate calculation is a matrix multiply for the result vector p:
    *
    *        [xpos(mm)             [bpm1x         //= BPMS:LTUS:660:X (mm)
    *    p=   ypos(mm)      = [F]*  bpm2x         //= BPMS:LTUS:680:X
    *         xang(mrad)            bpm3x         //= BPMS:LTUS:740:X
    *         yang(mrad)]           bpm4x         //= BPMS:LTUS:750:X
    *                               bpm1y         //= BPMS:LTUS:660:Y
    *                               bpm2y         //= BPMS:LTUS:680:Y
    *                               bpm3y         //= BPMS:LTUS:740:Y
    *                               bpm4y]        //= BPMS:LTUS:750:Y
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
    /* BMPOSITION1X: BlobSet mask bit 0x0008 */
    [BMPOSITION1X] = { name: "BPMS:LTUS:660:X"    , blob: 0, aMsk: AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y },	/* Position in mm/mrad */
    /* BMPOSITION2X: BlobSet mask bit 0x0010 */
    [BMPOSITION2X] = { name: "BPMS:LTUS:680:X"    , blob: 0, aMsk: AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y },	/* Position in mm/mrad */
    /* BMPOSITION3X: BlobSet mask bit 0x0020 */
    [BMPOSITION3X] = { name: "BPMS:LTUS:740:X"    , blob: 0, aMsk: AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y },	/* Position in mm/mrad */
    /* BMPOSITION4X: BlobSet mask bit 0x0040 */
    [BMPOSITION4X] = { name: "BPMS:LTUS:750:X"    , blob: 0, aMsk: AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y },	/* Position in mm/mrad */
    /* BC2CHARGE:    BlobSet mask bit 0x0080 */
    [BC2CHARGE]    = { name: "BLEN:LI24:886:BIMAX", blob: 0, aMsk: AVAIL_BC2CHARGE },	/* BC2 Charge in Amps */
    /* BC2ENERGY:    BlobSet mask bit 0x0100 */
    [BC2ENERGY]    = { name: "BPMS:LI24:801:X"    , blob: 0, aMsk: AVAIL_BC2ENERGY },	/* BC2 Energy in mm */
    /* BC1CHARGE:    BlobSet mask bit 0x0200 */
    [BC1CHARGE]    = { name: "BLEN:LI21:265:AIMAX", blob: 0, aMsk: AVAIL_BC1CHARGE },	/* BC1 Charge in Amps */
    /* BC1ENERGY:    BlobSet mask bit 0x0400 */
    [BC1ENERGY]    = { name: "BPMS:LI21:233:X"    , blob: 0, aMsk: AVAIL_BC1ENERGY },	/* BC1 Energy in mm */

    /**
    * Soft Undulator Launch 120Hz Feedback States X, X', Y, Y' (running on FB03:TR02)
    */
    /* UNDSTATE:    BlobSet mask bit 0x0800 */
    [UNDSTATE]     = { name: "FBCK:FB03:TR02:STATES", blob: 0, aMsk: AVAIL_UNDSTATE },

    /**
    * Charge at the DMP
    */
    /* DMP_CHARGE: BlobSet mask bit 0x1000 */
    [DMP_CHARGE]   = { name: "BPMS:DMPS:502:TMIT", blob: 0, aMsk: AVAIL_DMP_CHARGE },

    /**
    * XTCAV Voltage and Phase
    */
    /* XTCAV_AMP:    BlobSet mask bit 0x2000 */
    /* TODO (rreno): There does not seem to be an equivalent PV for DMPS in LLRF */
    [XTCAV_AMP]    = { name: "TCAV:DMPH:360:AV", blob: 0, aMsk: AVAIL_XTCAV_AMP },

};
#else
BLDBLOB bldPulseBlobs[] =
{
  /**
   * Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
   */
  /* BMCHARGE: BlobSet mask bit 0x0001 */
  [BMCHARGE] = { name: "BPMS:IN20:221:TMIT", blob: 0, aMsk: AVAIL_BMCHARGE},	/* Charge in Nel, 1.602e-10 nC per Nel*/

  /**
   * Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
   * where E0 is the final desired energy at the LTU (the magnet setting BEND:LTUH:125:BDES*1000)
   * dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
   * BPM1x = [BPMS:LTUH:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
   * BPM2x = [BPMS:LTUH:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
   */
  /* BMENERGY1X: BlobSet mask bit 0x0002 */
    [BMENERGY1X] = { name: "BPMS:LTUH:250:X", blob: 0, aMsk: AVAIL_BMENERGY1X},	/* Actually X pos in mm */
  /* BMENERGY2X: BlobSet mask bit 0x0004 */
    [BMENERGY2X] = { name: "BPMS:LTUH:450:X", blob: 0, aMsk: AVAIL_BMENERGY2X},	/* Actually X pos in mm */

  /**
   * Position X, Y, Angle X, Y at LTU:
   * Using the LTU Feedback BPMs: BPMS:LTUH:720,730,740,750
   * The best estimate calculation is a matrix multiply for the result vector p:
   *
   *        [xpos(mm)             [bpm1x         //= BPMS:LTUH:720:X (mm)
   *    p=   ypos(mm)      = [F]*  bpm2x         //= BPMS:LTUH:730:X
   *         xang(mrad)            bpm3x         //= BPMS:LTUH:740:X
   *         yang(mrad)]           bpm4x         //= BPMS:LTUH:750:X
   *                               bpm1y         //= BPMS:LTUH:720:Y
   *                               bpm2y         //= BPMS:LTUH:730:Y
   *                               bpm3y         //= BPMS:LTUH:740:Y
   *                               bpm4y]        //= BPMS:LTUH:750:Y
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
/* BMPOSITION1X: BlobSet mask bit 0x0008 */
  [BMPOSITION1X] = { name: "BPMS:LTUH:720:X"    , blob: 0, aMsk: AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y },	/* Position in mm/mrad */
/* BMPOSITION2X: BlobSet mask bit 0x0010 */
  [BMPOSITION2X] = { name: "BPMS:LTUH:730:X"    , blob: 0, aMsk: AVAIL_BMPOSITION2X | AVAIL_BMPOSITION2Y },	/* Position in mm/mrad */
/* BMPOSITION3X: BlobSet mask bit 0x0020 */
  [BMPOSITION3X] = { name: "BPMS:LTUH:740:X"    , blob: 0, aMsk: AVAIL_BMPOSITION3X | AVAIL_BMPOSITION3Y },	/* Position in mm/mrad */
/* BMPOSITION4X: BlobSet mask bit 0x0040 */
  [BMPOSITION4X] = { name: "BPMS:LTUH:750:X"    , blob: 0, aMsk: AVAIL_BMPOSITION4X | AVAIL_BMPOSITION4Y },	/* Position in mm/mrad */
/* BC2CHARGE:    BlobSet mask bit 0x0080 */
  [BC2CHARGE]    = { name: "BLEN:LI24:886:BIMAX", blob: 0, aMsk: AVAIL_BC2CHARGE },	/* BC2 Charge in Amps */
/* BC2ENERGY:    BlobSet mask bit 0x0100 */
  [BC2ENERGY]    = { name: "BPMS:LI24:801:X"    , blob: 0, aMsk: AVAIL_BC2ENERGY },	/* BC2 Energy in mm */
/* BC1CHARGE:    BlobSet mask bit 0x0200 */
  [BC1CHARGE]    = { name: "BLEN:LI21:265:AIMAX", blob: 0, aMsk: AVAIL_BC1CHARGE },	/* BC1 Charge in Amps */
/* BC1ENERGY:    BlobSet mask bit 0x0400 */
  [BC1ENERGY]    = { name: "BPMS:LI21:233:X"    , blob: 0, aMsk: AVAIL_BC1ENERGY },	/* BC1 Energy in mm */

  /**
   * Undulator Launch 120Hz Feedback States X, X', Y, Y' (running on FB03:TR05)
   * scondam: 11-Nov-2014: L.Piccoli moved Und Launch Feeback to FB05:TR05
   * Blob name changed from FBCK:FB03:TR05:STATES
   */
/* UNDSTATE:    BlobSet mask bit 0x0800 */
  [UNDSTATE]     = { name: "FBCK:FB05:TR05:STATES", blob: 0, aMsk: AVAIL_UNDSTATE},

  /**
   * Charge at the DMP
   */
/* DMP_CHARGE: BlobSet mask bit 0x1000 */
  [DMP_CHARGE]     = { name: "BPMS:DMPH:502:TMIT", blob: 0, aMsk: AVAIL_DMP_CHARGE},

  /**
   * XTCAV Voltage and Phase
   */
/* XTCAV_AMP:    BlobSet mask bit 0x2000 */
  [XTCAV_AMP]  = { name: "TCAV:DMPH:360:AV", blob: 0, aMsk: AVAIL_XTCAV_AMP},

};
#endif // BLD_SXR

#define N_PULSE_BLOBS (sizeof(bldPulseBlobs)/sizeof(bldPulseBlobs[0]))

FcomID fcomBlobIDs[N_PULSE_BLOBS] = { 0 };

FcomBlobSetRef  bldBlobSet = 0;


static epicsInt32 bldUseFcomSet = 0;

static unsigned int dataAvailable = 0;

int EBEAM_ENABLE = 1;
epicsExportAddress(int, EBEAM_ENABLE);

extern	int EORBITS_ENABLE;

int BLD_MCAST_ENABLE = 0;
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
epicsUInt32  bldGoodBlobCount[N_PULSE_BLOBS] = {0};
epicsUInt32  bldInvalidAlarmCount[N_PULSE_BLOBS] = {0};
epicsUInt32  bldUnmatchedTSCount[N_PULSE_BLOBS]  = {0};
epicsUInt32  bldFcomTimeoutCount[N_PULSE_BLOBS]  = {0};
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
	/* This function is called via an initHook after iocInit() */
    printf( "BLDMCastStart: %s %s\n", (enable ? "enable" : "disable"), NIC );

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
void EVRFire( void * pBlobSet )
{
	epicsTimeStamp time40;
/*	int		fidlast, fidLast40; */
	int		fid40, fidpipeline;
	unsigned long long	tscLast;
	/* evrRWMutex is locked while calling these user functions so don't do anything that might block. */
	epicsTimeStamp time_s;

	/* get the current pattern data - check for good status */
	evrModifier_ta modifier_a;
	epicsUInt32  patternStatus; /* see evrPattern.h for values */
	int status = evrTimeGetFromPipeline(&time_s,  evrTimeCurrent, modifier_a, &patternStatus, 0,0,0);
	if ( status != 0 )
	{
		/* Error from evrTimeGetFromPipeline! */
		bldFiducialTime.nsec = PULSEID_INVALID;
		return;
	}
#if defined(BLD_SXR)
	/* check for LCLS SXR beam */
	if ( ((modifier_a[MOD3_IDX] & BKRCUS) == 0) ||
         ((modifier_a[MOD5_IDX] & MOD5_BEAMFULL_MASK) == 0) )
#else
	/* check for LCLS HXR beam */
	if ( ((modifier_a[MOD3_IDX] & BKRCUS) != 0) ||
         ((modifier_a[MOD5_IDX] & MOD5_BEAMFULL_MASK) == 0) )
#endif
	{
		/* This is 360Hz. So printf will really screw timing. Only enable briefly */
		if(BLD_MCAST_DEBUG >= 6)
            errlogPrintf("EVR fires (status %i, mod5 0x%08x, fid %d)\n",
                    status, (unsigned)modifier_a[MOD5_IDX], PULSEID(time_s) );
		/* No beam */
		return;
	}

	fidpipeline = PULSEID(time_s);
	tscLast	= evrGetFiducialTsc();
	epicsTimeGetEvent( &time40, 40 );
	fid40 = PULSEID(time40);

	/* Get timestamps for beam fiducial */
	bldFiducialTime = time_s;
	bldFiducialTsc	= GetHiResTicks();

	if(BLD_MCAST_DEBUG >= 5) {
		double			deltaLastFid;
		deltaLastFid	= HiResTicksToSeconds( bldFiducialTsc - tscLast   ) * 1e3;
		errlogPrintf( "pipeline fid %d (-%0.2f)\n", fidpipeline, deltaLastFid );
	/* HACK */
		return;
	}

	/* This is 120Hz. So printf will screw timing. Only enable briefly. */
	if(BLD_MCAST_DEBUG >= 4) errlogPrintf("EVR fires (status %i, mod5 0x%08x, fid %d, fid40 %d)\n", status, (unsigned)modifier_a[4], fidpipeline, fid40 );

	/* Signal the EVRFireEvent to trigger the fcomGetBlobSet call */
	epicsEventSignal( EVRFireEvent);

#ifdef RTEMS
	if ( pBlobSet == NULL ) {
		BSP_timer_start( BSPTIMER, timer_delay_clicks );
	}
#endif
	return;
}

#ifdef RTEMS
/* Invoked by BSP_timer on 5ms timeout after fiducial */
/* Wakes up BLDMCast thread which calls fcomGetBlobSet() */
/* Only used if we're not using fcom SYNC mode */
static void evr_timer_isr(void *arg)
{/* post event/release sema to wakeup worker task here */
  if(EVRFireEvent) {
    epicsEventSignal(EVRFireEvent);
  }
    return;
}
#endif

static void printChIdInfo( chid caChId, char *message )
{
	if ( message && strlen(message) > 0 )
    	errlogPrintf("\n%s\n",message);
    errlogPrintf(	"pv: %s  type(%d) nelements(%ld) host(%s)\n",
					ca_name(caChId), ca_field_type(caChId),
					ca_element_count(caChId), ca_host_name(caChId) );
    errlogPrintf(	"    read(%d) write(%d) state(%d)\n",
					ca_read_access(caChId), ca_write_access(caChId),
					ca_state(caChId) );
}

static void exceptionCallback( struct exception_handler_args args )
{
    chid	caChId = args.chid;
    long	stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (caChId ? ca_name(caChId) : noname);

    if(caChId) printChIdInfo(caChId,"exceptionCallback");
    errlogPrintf("exceptionCallback stat %s channel %s\n", ca_message(stat),channel);
}

static void eventCallback( struct event_handler_args args )
{
    BLDPV * pPV = args.usr;

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
    	if(BLD_MCAST_DEBUG >= 1) errlogPrintf("Event Callback: %s, CA Error %s\n", ca_name(args.chid), ca_message(args.status) );
		dataAvailable &= ~ pPV->availMask;
    }
    epicsMutexUnlock(bldMutex);
}

/****************************************************************************
 *
 * Function:	connectionCallback
 *
 * Description:	CA connectionCallback
 *
 * Arg(s) In:	args  -  connection_handler_args (see CA manual)
 *
 **************************************************************************-*/
static void connectionCallback( struct connection_handler_args args )
{
    BLDPV	*	pPv	= ( BLDPV * ) ca_puser ( args.chid );
    if ( args.op == CA_OP_CONN_UP )
	{
		if ( BLD_MCAST_DEBUG >= 2 ) {
			printf("CA connection up: %s\n", pPv->name ); fflush(stdout);
		}
        if ( !pPv->subscribed )
		{
			unsigned long		cnt;

            /* Set up pv structure */
            /* ------------------- */

			/* BLD CA connections only support native data type double */
			if ( DBF_DOUBLE != ca_field_type( pPv->caChId ) )
			{
				errlogPrintf( "Native data type of '%s' is not double.\n", pPv->name );
			}

			/* Check request count */
			cnt = ca_element_count( pPv->caChId );
			if ( pPv->nElems != cnt )
			{
				errlogPrintf("Number of elements [%ld] of '%s' does not match expectation.\n", cnt, pPv->name);
			}

			/* Allocate time_double storage for each PV element */
			if ( pPv->pTD == NULL )
			{
				pPv->pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, pPv->nElems), "callocMustSucceed");
			}

            /* Create subscription on first connect */
            pPv->status = ca_create_subscription(	DBR_TIME_DOUBLE, pPv->nElems, pPv->caChId,
                                                	DBE_VALUE | DBE_ALARM, eventCallback, pPv, NULL );
			SEVCHK( pPv->status, "ca_create_subscription");
			if ( pPv->status == ECA_NORMAL ) {
            	pPv->subscribed = 1;
				if ( BLD_MCAST_DEBUG >= 3 ) {
					printf("Subscribed to CA monitor: %s\n", pPv->name ); fflush(stdout);
				}
			}
		}
    }
    else if ( args.op == CA_OP_CONN_DOWN )
	{
		if ( BLD_MCAST_DEBUG >= 2 ) {
			printf("CA connection down: %s\n", pPv->name ); fflush(stdout);
		}
        pPv->status = ECA_DISCONN;
    }
}

/* was connectCaPv(const char *name, chid *p_chid) */
static int
connectCaPv( BLDPV * pPv )
{
	int rtncode;

	if ( pPv == NULL )
		return -1;

	rtncode = ECA_NORMAL;

	/* We need a connectionCallback as CA PV's may come and go while IOC is running */
	if ( BLD_MCAST_DEBUG >= 1 ) {
		printf("Creating CA channel: %s\n", pPv->name ); fflush(stdout);
	}

	pPv->subscribed	= 0;
	pPv->status		= ECA_DISCONN;
	rtncode = ca_create_channel( pPv->name, connectionCallback, pPv, CA_PRIORITY , &pPv->caChId );
	SEVCHK( rtncode, "ca_create_channel" );
	if ( rtncode != ECA_NORMAL )
	{
		printf( "Error: ca_create_channel failed for %s\n", pPv->name );
		return -1;
	}

	if ( BLD_MCAST_DEBUG >= 3 ) {
		printf(" ca_create_channel successful for %s\n", pPv->name );
		fflush(stdout);
	}

#if 0
	{
	/* We could do subscription in connection callback. But in this case, better to enforce all connection */
	if ( BLD_MCAST_DEBUG >= 2 ) {
		printf("pending for IO ...\n");
		fflush(stdout);
	}
	rtncode = ca_pend_io(10.0);
	if ( ECA_NORMAL != rtncode )
	{
		ca_clear_channel( pPV->caChId );
		pPv->caChId = 0;
		if (rtncode == ECA_TIMEOUT) {
			errlogPrintf("Channel connect timed out: '%s' not found.\n", pPv->name);
			fflush(stdout);
			if ( ! bldConnectAbort ) {
				errlogPrintf("Continuing to try -- set bldConnectAbort nonzero to abort\n");
				continue;
			}
		} else {
			errlogPrintf("ca_pend_io() returned %i\n", rtncode);
		}
		return -1;
	}
	}
#endif

	return 0;
}

static int
init_pvarray(BLDPV *p_pvs, int n_pvs, int subscribe)
{
	int           loop;
	unsigned int	nFailedConnections = 0;

	for(loop=0; loop<n_pvs; loop++)
	{
		if ( connectCaPv( &p_pvs[loop] ) ) {
			nFailedConnections++;
			continue;
		}

#if 0
		{
		unsigned long cnt;
		if ( p_pvs[loop].nElems != (cnt = ca_element_count(p_pvs[loop].caChId)) ) {
			errlogPrintf("Number of elements [%ld] of '%s' does not match expectation.\n", cnt, p_pvs[loop].name);
			return -1;
		}

		if ( DBF_DOUBLE != ca_field_type(p_pvs[loop].caChId) ) {/* Native data type has to be double */
			errlogPrintf("Native data type of '%s' is not double.\n", p_pvs[loop].name);
			return -1;
		}

		/* Everything should be double, even not, do conversion */
		p_pvs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, p_pvs[loop].nElems), "callocMustSucceed");

		if ( subscribe ) {
			SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, p_pvs[loop].nElems, p_pvs[loop].caChId, DBE_VALUE|DBE_ALARM, eventCallback, &(p_pvs[loop]), NULL), "ca_create_subscription");
		}
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
        if(bldStaticPVs[loop].caChId)
        {
            ca_clear_channel(bldStaticPVs[loop].caChId);
            bldStaticPVs[loop].caChId = NULL;
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
struct sockaddr_in sockaddrDst;
#endif
unsigned char mcastTTL;
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

	if ( (rtncode = udpCommConnect( sFd, inet_addr(getenv("BLDMCAST_DST_IP")), BLDMCAST_DST_PORT )) ) {
		errlogPrintf("Failed to 'connect' to peer: %s\n", strerror(-rtncode));
		epicsMutexDestroy( bldMutex );
		bldMutex = 0;
		udpCommClose( sFd );
		return -1;
	}

	mcastTTL = MCAST_TTL;
	if ( ( rtncode = udpCommSetMcastTTL( sFd, mcastTTL ) ) ) {
		errlogPrintf("Failed to set multicast TTL: %s\n", strerror(-rtncode));
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

	memset(&sockaddrDst, 0, sizeof(struct sockaddr_in));

	sockaddrDst.sin_family      = AF_INET;
	sockaddrDst.sin_addr.s_addr = inet_addr(getenv("BLDMCAST_DST_IP"));
	sockaddrDst.sin_port        = htons(BLDMCAST_DST_PORT);

#if 0	/* bind should not be necessary as this is xmit only */
	{
	struct sockaddr_in sockaddrSrc;
	memset(&sockaddrSrc, 0, sizeof(struct sockaddr_in));
	sockaddrSrc.sin_family      = AF_INET;
	sockaddrSrc.sin_addr.s_addr = INADDR_ANY;
	sockaddrSrc.sin_port        = 0;
	if( bind( sFd, (struct sockaddr *) &sockaddrSrc, sizeof(struct sockaddr_in) ) == -1 )
	{
		errlogPrintf("Failed to bind local socket for multicast\n");
		close(sFd);
		epicsMutexDestroy(bldMutex);
		return -1;
	}
	}
#endif

	if(BLD_MCAST_DEBUG >= 2)
	{
		struct sockaddr_in	sockaddrName;
		socklen_t			iLength = sizeof(sockaddrName);
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
	/* printf("All PVs are successfully connected!\n"); Maybe, maybe not. */

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
					if(BLD_MCAST_DEBUG >= 3) errlogPrintf("Timed out waiting for Beam event\n");
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
			int		getBlobTime	= usSinceFiducial();
			if(BLD_MCAST_DEBUG >= 4) errlogPrintf("Calling fcomGetBlobSet w/ timeout %u ms at FID+%d us\n", (unsigned int) timer_delay_ms, getBlobTime );
			t_HiResTime		tickBeforeGet	= GetHiResTicks();
			/* Get all the blobs w/ timeout timer_delay_ms */
			FcomBlobSetMask	waitFor	= (1<<N_PULSE_BLOBS) - 1;
			status = fcomGetBlobSet( bldBlobSet, &got_mask, waitFor, FCOM_SET_WAIT_ALL, timer_delay_ms );
			t_HiResTime		tickAfterGet	= GetHiResTicks();
			double			usInFcom		= HiResTicksToSeconds( tickAfterGet - tickBeforeGet ) * 1e6;
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
				if(BLD_MCAST_DEBUG >= 3) errlogPrintf("fcomGetBlobSet %u ms timeout in %.1fus! req 0x%X, got 0x%X\n", (unsigned int) timer_delay_ms, usInFcom, (unsigned int)waitFor, (unsigned int)got_mask );
			}
			else
			{
				if(BLD_MCAST_DEBUG >= 4) errlogPrintf( "fcomGetBlobSet succeeded in %.1fus.\n", usInFcom );
			}
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
					if ( rtncode ) {
						bldFcomTimeoutCount[loop]++;
						if ( BLD_MCAST_DEBUG >= 3 )
							errlogPrintf(	"Blob %s timeout, no blob for %u sec, %u fid\n",
											bldPulseBlobs[loop].name,
											p_refTime->secPastEpoch, p_refTime->nsec & PULSEID_INVALID );
					}
				} else {
					/* No blob set, just fetch current blob status one by one */
					rtncode = fcomGetBlob( fcomBlobIDs[loop], &bldPulseBlobs[loop].blob, 0 );
					if ( rtncode == FCOM_ERR_NO_DATA ) {
						bldFcomTimeoutCount[loop]++;
						if ( BLD_MCAST_DEBUG >= 3 )
							errlogPrintf(	"Blob %s timeout, no blob for %u sec, %u fid\n",
											bldPulseBlobs[loop].name,
											p_refTime->secPastEpoch, p_refTime->nsec & PULSEID_INVALID );
					} else if ( rtncode )
						bldFcomGetErrs[loop]++;
				}

				if ( rtncode ) {
					dataAvailable &= ~bldPulseBlobs[loop].aMsk;
					bldPulseBlobs[loop].blob = 0;
					continue;
				}

				/* Grab a reference to this blob and check it's status */
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
					if ( BLD_MCAST_DEBUG >= 2 )
						errlogPrintf(	"Blob %s tsMismatch %u sec, %u fid instead of %u sec, %u fid\n",
										bldPulseBlobs[loop].name,
										(unsigned int) (b1->fc_tsHi),
										(unsigned int) (b1->fc_tsLo	 & PULSEID_INVALID),
										p_refTime->secPastEpoch, p_refTime->nsec & PULSEID_INVALID );
					bldUnmatchedTSCount[loop]++;
					dataAvailable &= ~bldPulseBlobs[loop].aMsk;
				}
				else {
					bldGoodBlobCount[loop]++;
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

#define AVAIL_LTUPOS	\
	(	AVAIL_BMPOSITION1X | AVAIL_BMPOSITION1Y | \
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
					acc += pMatrixValue[0] * bldPulseBlobs[BMPOSITION1X].blob->fcbl_bpm_X;
					acc += pMatrixValue[1] * bldPulseBlobs[BMPOSITION1X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[2] * bldPulseBlobs[BMPOSITION2X].blob->fcbl_bpm_X;
					acc += pMatrixValue[3] * bldPulseBlobs[BMPOSITION2X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[4] * bldPulseBlobs[BMPOSITION3X].blob->fcbl_bpm_X;
					acc += pMatrixValue[5] * bldPulseBlobs[BMPOSITION3X].blob->fcbl_bpm_Y;
					acc += pMatrixValue[6] * bldPulseBlobs[BMPOSITION4X].blob->fcbl_bpm_X;
					acc += pMatrixValue[7] * bldPulseBlobs[BMPOSITION4X].blob->fcbl_bpm_Y;

					tempDA[i] = acc;
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
			if( AVAIL_XTCAV_AMP & dataAvailable )
			{
			  if(BLD_XTCAV_DEBUG >= 1) {
			    errlogPrintf("Got XTCAV blob %f %f\n", (double)bldPulseBlobs[XTCAV_AMP].blob->fcbl_llrf_aavg,
					 (double)bldPulseBlobs[XTCAV_AMP].blob->fcbl_llrf_pavg);
			  }

			  __st_le64(&bldEbeamInfo.ebeamXTCAVAmpl, (double)bldPulseBlobs[XTCAV_AMP].blob->fcbl_llrf_aavg);
			  __st_le64(&bldEbeamInfo.ebeamXTCAVPhase, (double)bldPulseBlobs[XTCAV_AMP].blob->fcbl_llrf_pavg);
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
			if( AVAIL_DMP_CHARGE & dataAvailable )
			{
			  __st_le64(&bldEbeamInfo.ebeamDMP502Charge, (double)bldPulseBlobs[DMP_CHARGE].blob->fcbl_bpm_T);
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
				x450 corresponds to the bpm x position data  from BPMS:LTUH:450:X
				x250 corresponds to the bpm x position data  from BPMS:LTUH:250:X

				BPMS:LTUH:450:X and BPMS:LTUH:250:X both arrive on every pulse over FCOM.

				eVave = SIOC:SYS0:ML00:AO627;      % a single number - Slow update (1 sec or so) over CA from matlab and from subroutine record

				% Slow update (1 sec or so) over CA from matlab - typically there should be the last few hundred data points in x450 or x250
				x450ave = SIOC:SYS0:ML02:AO041;    % from subroutine record
				x250ave = SIOC:SYS0:ML02:AO040;	   % from subroutine record
				x450 = BPMS:LTUH:450:X;
                x250 = BPMS:LTUH:250:X;

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
				double x450 = bldPulseBlobs[BMENERGY2X].blob->fcbl_bpm_X;	/* BPMS:LTUH:450:X */
				double x250 = bldPulseBlobs[BMENERGY1X].blob->fcbl_bpm_X;	/* BPMS:LTUH:250:X */
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

			/* Shantha Condamoor: 7-Jul-2014: shot-to-shot values of X positions of LTUH BPMS 250 and 450 sent in eBeam BLD data */

			/* BPM LTUH 450 and 250 X positions */

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

		/*
		 * This scanIoRequest triggers I/O Scan processing of all the BLD_SNDR DTYP records.
		 * It's getting triggered after the FCOM Blob set fetch, approx 3ms to 5ms after beam,
		 * but before we send the BLD packet below.
		 * As the BLD_SNDR records get their data from the most recently recieved BLD packet,
		 * it's going to be the BLD for the prior pulse.
		 * Ran a number of tests on lcls-srv01 comparing original src PV's, their 10HZ BSA buffers,
		 * along w/ their counterparts from fcom blobs via multicast BLD and the
		 * timestamps and data values match for all of them.  Since each data point gets
		 * the right timestamp it shouldn't matter that we're publishing it on the next pulse.
		 */
		scanIoRequest(bldIoscan);

#ifdef MULTICAST
		/* do MultiCast */
		if ( BLD_MCAST_ENABLE )
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
		rtncode = fcomUnsubscribe( fcomBlobIDs[loop] );
		if ( rtncode )
			fprintf(stderr, "Unable to unsubscribe %s from FCOM: %s\n", bldPulseBlobs[loop].name, fcomStrerror(rtncode));
	}

	/*Should never return from following call*/
	/*SEVCHK(ca_pend_event(0.0),"ca_pend_event");*/
	return(0);
}

void bldDumpStats(FILE *f, int reset)
{
	int      i;
	epicsUInt32 cnt [N_PULSE_BLOBS];
	epicsUInt32 cnt1[N_PULSE_BLOBS];
	epicsUInt32 cnt2[N_PULSE_BLOBS];
	epicsUInt32 cnt3[N_PULSE_BLOBS];
	epicsUInt32 cnt4[N_PULSE_BLOBS];

	if ( bldMutex ) epicsMutexLock( bldMutex );
		memcpy(cnt,  bldUnmatchedTSCount,  sizeof(cnt));
		memcpy(cnt1, bldInvalidAlarmCount, sizeof(cnt1));
		memcpy(cnt2, bldFcomGetErrs,       sizeof(cnt2));
		memcpy(cnt3, bldFcomTimeoutCount,  sizeof(cnt3));
		memcpy(cnt4, bldGoodBlobCount,     sizeof(cnt4));
	if ( bldMutex ) epicsMutexUnlock( bldMutex );
	if ( !f )
		f = stdout;
	fprintf(f,"FCOM Good Blob count:\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt4[i]);
	}
	fprintf(f,"FCOM Data timestamp mismatch:\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt[i]);
	}
	fprintf(f,"FCOM Data timeout:\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt3[i]);
	}
	fprintf(f,"FCOM Data with bad/invalid status\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt1[i]);
	}
	fprintf(f,"FCOM 'get' message errors\n");
	for ( i=0; i<N_PULSE_BLOBS; i++ ) {
		fprintf(f,"  %40s: %9lu times\n", bldPulseBlobs[i].name, (unsigned long)cnt2[i]);
	}
	fprintf(f,"BLD MCAST messages posted: %10lu\n", (unsigned long)bldMcastMsgSent);
	if ( reset ) {
		if ( bldMutex ) epicsMutexLock( bldMutex );
			for ( i=0; i<N_PULSE_BLOBS; i++ ) {
				bldFcomTimeoutCount[i]  = 0;
				bldUnmatchedTSCount[i]  = 0;
				bldInvalidAlarmCount[i] = 0;
				bldFcomGetErrs[i]       = 0;
				bldGoodBlobCount[i]		= 0;
			}
			bldMcastMsgSent = 0;
		if ( bldMutex ) epicsMutexUnlock( bldMutex );
	}
}

static const iocshArg		bldDumpStatsArg0	= { "filename", iocshArgString };
static const iocshArg		bldDumpStatsArg1	= { "reset",	iocshArgInt };
static const iocshArg	*	bldDumpStatsArgs[2]	= { &bldDumpStatsArg0, &bldDumpStatsArg1 };
static const iocshFuncDef	bldDumpStatsFuncDef = { "bldDumpStats", 2,  bldDumpStatsArgs };
static void bldDumpStatsCallFunc( const iocshArgBuf * args )
{
	FILE	*	fp	= NULL;
	if ( args[0].sval != NULL )
	{
		fp = fopen( args[0].sval, "w" );
		if ( fp == NULL )
			printf( "bldDumpStats Error: Unable to open %s\n", args[0].sval );
	}
	bldDumpStats( fp, args[1].ival );
}


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

extern void  eOrbitsStart( initHookState state );

void  initHookBLDMCastStart( initHookState state )
{
	if ( state == initHookAfterIocRunning )
	{
		const char * strIP_BLD_SEND = getenv("IP_BLD_SEND");
		if ( strIP_BLD_SEND == NULL || strlen(strIP_BLD_SEND) == 0 )
		{
			printf( "initHookBLDMCastStart Error: IP_BLD_SEND env var not set!" );
			return;
		}
		printf( "initHookBLDMCastStart: IP_BLD_SEND is %s\n", strIP_BLD_SEND );
		BLDMCastStart( 1, strIP_BLD_SEND );
	}
}

/* implementation */
static long BLD_EPICS_Init()
{
	int loop;
	int rtncode;
	int enable_broadcast = 1;

	printf("BLD_EPICS_Init: Intializing BLD driver.\n" );

	if ( enable_broadcast ) {
		int name_len = 100;
		char name[name_len];
		rtncode = gethostname(name, name_len);
		if (rtncode == 0) {
			printf("INFO: BLD hostname is %s\n", name);
		}
		else {
			printf("ERROR: Unable to get hostname (errno=%d, rtncode=%d)\n", errno, rtncode);
			enable_broadcast = 0;
		}
	}

	if ( BLD_MCAST_ENABLE ) {
#if 0
		fcomUtilFlag = DP_DEBUG;
#endif
		for ( loop=0; loop < N_PULSE_BLOBS; loop++) {
			if ( BLD_MCAST_DEBUG >= 3 ) printf( "INFO: Looking up fcom ID for %s\n", bldPulseBlobs[loop].name );
			if ( FCOM_ID_NONE == (fcomBlobIDs[loop] = fcomLCLSPV2FcomID(bldPulseBlobs[loop].name)) ) {
				errlogPrintf("FATAL ERROR: Unable to determine FCOM ID for PV %s\n",
						 bldPulseBlobs[loop].name );
				return -1;
			}
			/* Why are we not using FCOM_SYNC_GET here? */
			rtncode = fcomSubscribe( fcomBlobIDs[loop], FCOM_ASYNC_GET );
			if ( 0 != rtncode ) {
				errlogPrintf("FATAL ERROR: Unable to subscribe %s (0x%08"PRIx32") to FCOM: %s\n",
						bldPulseBlobs[loop].name,
						fcomBlobIDs[loop],
						fcomStrerror(rtncode));
				return -1;
			}
			printf( "INFO: Subscribed to fcom ID 0x%X, %s\n",
					(unsigned int) fcomBlobIDs[loop], bldPulseBlobs[loop].name );
		}

		/* Allocate blob set here so that the flag that is read
		 * into a record already contains the final value, ready
		 * for being picked up by PINI
		 */
		if ( (rtncode = fcomAllocBlobSet( fcomBlobIDs, sizeof(fcomBlobIDs)/sizeof(fcomBlobIDs[0]), &bldBlobSet)) ) {
			errlogPrintf("ERROR: Unable to allocate blob set: %s; trying asynchronous mode\n", fcomStrerror(rtncode));
			bldBlobSet = 0;

		} else
		{
			bldUseFcomSet = 1;
	}

		if ( devBusMappedRegisterIO("bld_timer_io", &timer_delay_io) )
			errlogPrintf("ERROR: Unable to register I/O methods for timer delay\n"
						 "SOFTWARE MAY NOT WORK PROPERLY\n");

		/* These are simple local memory devBus devices for showing fcom status in PV's */
		checkDevBusMappedRegister("bld_stat_good", &bldGoodBlobCount);
		checkDevBusMappedRegister("bld_stat_bad_ts", &bldUnmatchedTSCount);
		checkDevBusMappedRegister("bld_stat_timeout", &bldFcomTimeoutCount);
		checkDevBusMappedRegister("bld_stat_bad_d", &bldInvalidAlarmCount);
		checkDevBusMappedRegister("bld_stat_bad_f", &bldFcomGetErrs);
		checkDevBusMappedRegister("bld_stat_msg_s", &bldMcastMsgSent);
		checkDevBusMappedRegister("bld_stat_min_f", &bldMinFcomDelayUs);
		checkDevBusMappedRegister("bld_stat_max_f", &bldMaxFcomDelayUs);
		checkDevBusMappedRegister("bld_stat_avg_f", &bldAvgFcomDelayUs);
		checkDevBusMappedRegister("bld_stat_max_p", &bldMaxPostDelayUs);
		checkDevBusMappedRegister("bld_stat_avg_p", &bldAvgPostDelayUs);
		checkDevBusMappedRegister("bld_fcom_use_s", &bldUseFcomSet);
	}

	scanIoInit(&bldIoscan);

	bldMutex = epicsMutexMustCreate();

	if (enable_broadcast && EBEAM_ENABLE ) {
		int	initHookStatus = initHookRegister( initHookBLDMCastStart );
		if ( initHookStatus != 0 )
			errlogPrintf( "Error %d from initHookRegister of BLDMCastStart!\n", initHookStatus );
	}
	else {
	  errlogPrintf("WARN: *** Not starting eBEAM BLDMCast task ***\n");
	}

	if (enable_broadcast && EORBITS_ENABLE ) {
		printf( "*** Starting eORBITS task ***\n");
		int	initHookStatus = initHookRegister( eOrbitsStart );
		if ( initHookStatus != 0 )
			errlogPrintf( "Error %d from initHookRegister of eOrbitsStart!\n", initHookStatus );
	}

	iocshRegister( &bldDumpStatsFuncDef, bldDumpStatsCallFunc );
	return 0;
}

static long BLD_EPICS_Report(int level)
{
    printf("\n"BLD_DRV_VERSION"\n\n");
	printf("Compiled with options:\n");

	printf("MULTICAST        :");
#ifdef MULTICAST
		printf(             "  DEFINED (use multicast interface to send data)\n");
#ifdef MULTICAST_UDPCOMM
	printf("MULTICAST_UDPCOMM:");
			printf(         "  DEFINED (use UDPCOMM, not BSD sockets for sending BLD multicast messages)\n");
#else
			printf(         "  DEFINED (use BSD sockets, not UDPCOMM for sending BLD multicast messages)\n");
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

	if ( level > 1 ) {
		unsigned int	loop;
    	for(loop=0; loop<N_STATIC_PVS; loop++) {
			chid	caChId	= bldStaticPVs[loop].caChId;
			if ( caChId )
				printChIdInfo( caChId, "" );
		}
	}

	if ( level > 2 ) {
		BLD_report_EBEAMINFO();
	}

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
  printf("uDamageMask: 0x%0X\n", (unsigned int) __ld_le32(&bldEbeamInfo.uDamageMask));
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
