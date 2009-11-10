/* BLDMCast.h */
#ifndef _BLD_MCAST_H_
#define _BLD_MCAST_H_

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"
#include "dbDefs.h"

#include <epicsTime.h>

#define BLD_DRV_VERSION "BLD driver V1.0"

#define CA_PRIORITY	CA_PRIORITY_MAX		/* Highest CA priority */

#define TASK_PRIORITY	epicsThreadPriorityMax	/* Highest EPICS thread priority */

#define DEFAULT_CA_TIMEOUT	0.04		/* Default CA timeout, for 30Hz */
#define DEFAULT_EVR_TIMEOUT	0.2		/* Default EVR event timeout, for 30Hz */

#define MAX_PV_NAME_LEN 40

#ifndef BOOL
#define BOOL int
#endif

/* Structure representing one PV (= channel) */
typedef struct BLDPV
{
    const char *	name;
    unsigned long	nElems; /* type is always DOUBLE */

    BOOL		dataAvailable;

    chid		pvChId;

    struct dbr_time_double * pTD;

/* No need for eventId, never cancel subscription *\
    evid  pvEvId;
\* No need for eventId, never cancel subscription */

/* No need to hold type, always double *\
    long  dbfType;
    long  dbrType;
\* No need to hold type, always double */
} BLDPV;

/* We monitor static PVs and update their value upon modification */
enum STATICPVSINDEX
{
    DSPR1=0,
    DSPR2,
    E0BDES,
    FMTRX
};/* the definition here must match the PV definition below */

static BLDPV staticPVs[]=
{
    {"BLD:SYS0:500:DSPR1", 1, FALSE, NULL, NULL},	/* For Energy */
    {"BLD:SYS0:500:DSPR2", 1, FALSE, NULL, NULL},	/* For Energy */
    {"BEND:LTU0:125:BDES", 1, FALSE, NULL, NULL},	/* Energy in MeV */
    {"BLD:SYS0:500:FMTRX", 32, FALSE, NULL, NULL}	/* For Position */
};
#define N_STATIC_PVS (sizeof(staticPVs)/sizeof(struct BLDPV))

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
};/* the definition here must match the PV definition below, the order is critival as well */

static BLDPV pulsePVs[]=
{
#if 0
    Charge (nC) = BPMS:IN20:221:TMIT (Nel) * 1.602e-10 (nC/Nel)   // [Nel = number electrons]
#endif
    {"BPMS:IN20:221:TMIT", 1, FALSE, NULL, NULL},	/* Charge in Nel, 1.602e-10 nC per Nel*/

#if 0
    Energy at L3 (MeV) = [ (BPM1x(MeV) + BPM2x(MeV))/2  ]*E0(MeV) + E0 (MeV)
    where E0 is the final desired energy at the LTU (the magnet setting BEND:LTU0:125:BDES*1000)
    dspr1,2  = Dx for the chosen dispersion BPMs (from design model database twiss parameters) (we can store these in BLD IOC PVs)
    BPM1x = [BPMS:LTU1:250:X(mm)/(dspr1(m/Mev)*1000(mm/m))]
    BPM2x = [BPMS:LTU1:450:X(mm)/(dspr2(m/Mev)*1000(mm/m))]
#endif
    {"BPMS:LTU1:250:X", 1, FALSE, NULL, NULL},	/* Energy in MeV */
    {"BPMS:LTU1:450:X", 1, FALSE, NULL, NULL},	/* Energy in MeV */

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

    {"BPMS:LTU1:720:X", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:730:X", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:740:X", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:750:X", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:720:Y", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:730:Y", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:740:Y", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */
    {"BPMS:LTU1:750:Y", 1, FALSE, NULL, NULL},	/* Position in mm/mrad */

    {"BLEN:LI24:886:BIMAX", 1, FALSE, NULL, NULL},	/* Bunch Length in Amps */
};
#define N_PULSE_PVS (sizeof(pulsePVs)/sizeof(struct BLDPV))

typedef struct EBEAMINFO
{
    epicsTimeStamp	timestamp;
    unsigned int uMBZ1;
    unsigned int uFiducialId;
    unsigned int uMBZ2;

    /* Xtc Section 1 */
    unsigned int uDamage;
    unsigned int uLogicalId; /* source 1 */
    unsigned int uPhysicalId;/* source 2 */
    unsigned int uDataType;  /* Contains */
    unsigned int uExtentSize;/* Extent */

    /* Xtc Section 2 */
    unsigned int uDamage2;
    unsigned int uLogicalId2;
    unsigned int uPhysicalId2;
    unsigned int uDataType2;
    unsigned int uExtentSize2;

    unsigned int uDamageMask;

    double	ebeamCharge;	/* in nC */
    double	ebeamL3Energy;	/* in MeV */
    double	ebeamLTUPosX;	/* in mm */
    double	ebeamLTUPosY;	/* in mm */
    double	ebeamLTUAngX;	/* in mrad */
    double	ebeamLTUAngY;	/* in mrad */

    double	ebeamBunchLen;	/* in Amps */
} EBEAMINFO;

#define EBEAM_INFO_ERROR 0x4000

static EBEAMINFO ebeamInfoPreFill =
{
    {0,0},/* to be changed pulse by pulse */
    0,
    0,	/* to be changed pulse by pulse, low 17 bits of timestamp nSec */
    0,

    0, /* to be changed, if error, 0x4000 */
    0x06000000,
    0,
    0x1000f,
    72,

    0, /* to be changed, if error, 0x4000 */
    0x06000000,
    0,
    0x1000f,
    72,

    0, /* to be changed, mask */

    0.0,
    0.0,
    0.0,
    0.0,
    0.0,
    0.0,

    0.0
};

#define MCAST_LOCAL_IP	"172.27.225.21"
#define MCAST_DST_IP	(inet_addr("239.255.24.0"))
#define MCAST_DST_PORT	(htons(10148))
#define MCAST_TTL	8
#endif
