/* $Id: BLDMCast.h,v 1.27 2014/04/28 23:17:40 scondam Exp $ */
#ifndef _BLD_MCAST_H_
#define _BLD_MCAST_H_

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsMutex.h>
#include <dbScan.h>

#include "BLDTypes.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Original version */
#define EBEAMINFO_VERSION_0 0x1000f

/* Addition of BC2ENERGY, BC1CHARGE and BC1ENERGY */
#define EBEAMINFO_VERSION_1 0x3000f

/* Addition of X, X', Y, Y' calculaded by the Undulator Launch 120Hz feedback */
#define EBEAMINFO_VERSION_2 0x4000f

/* Addition of BPMS:LTU1:[250/450]:X and BPMS:DMP1:502:TMIT */
#define EBEAMINFO_VERSION_3 0x5000f

/* Size of original data packet */
#define EBEAMINFO_VERSION_0_SIZE 80

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_1_SIZE (EBEAMINFO_VERSION_0_SIZE + sizeof(Flt64_LE) * 3)

/* Addition of 4 more floats */
#define EBEAMINFO_VERSION_2_SIZE (EBEAMINFO_VERSION_1_SIZE + sizeof(Flt64_LE) * 4)

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_3_SIZE (EBEAMINFO_VERSION_2_SIZE + sizeof(Flt64_LE) * 3)

/**
 * Structure defined in this document:
 * https://confluence.slac.stanford.edu/download/attachments/10256639/bldicd.pdf
 */
  typedef struct EBEAMINFO {
    Uint32_LE     ts_sec;
    Uint32_LE     ts_nsec;
    Uint32_LE     uMBZ1;
    Uint32_LE     uFiducialId;
    Uint32_LE     uMBZ2;

    /* Xtc Section 1 */
    Uint32_LE     uDamage;
    Uint32_LE     uLogicalId;   /* source 1 */
    Uint32_LE     uPhysicalId;  /* source 2 */
    Uint32_LE     uDataType;    /* Contains - this is the version field */
    Uint32_LE     uExtentSize;  /* Extent - size of data following Xtc Section 2*/

    /* Xtc Section 2 */
    Uint32_LE     uDamage2;
    Uint32_LE     uLogicalId2;
    Uint32_LE     uPhysicalId2;
    Uint32_LE     uDataType2;
    Uint32_LE     uExtentSize2; /* Extent - size of data following Xtc Section 2*/

    /* Data */
    Uint32_LE     uDamageMask;

    Flt64_LE      ebeamCharge;   /* in nC */
    Flt64_LE      ebeamL3Energy; /* in MeV */
    Flt64_LE      ebeamLTUPosX;  /* in mm */
    Flt64_LE      ebeamLTUPosY;  /* in mm */
    Flt64_LE      ebeamLTUAngX;  /* in mrad */
    Flt64_LE      ebeamLTUAngY;  /* in mrad */

    Flt64_LE      ebeamBC2Current; /* in Amps */

    /* Added in VERSION_1 */
    Flt64_LE      ebeamBC2Energy; /* in mm */
    Flt64_LE      ebeamBC1Current; /* in Amps */
    Flt64_LE      ebeamBC1Energy; /* in mm */

    /* Added in VERSION_2 */
    Flt64_LE      ebeamUndPosX; /* in mm */
    Flt64_LE      ebeamUndPosY; /* in mm */
    Flt64_LE      ebeamUndAngX; /* in mrad */
    Flt64_LE      ebeamUndAngY; /* in mrad */

    /* Added in VERSION_3 */
    Flt64_LE      ebeamXTCAVAmpl; /* in MeV */
    Flt64_LE      ebeamXTCAVPhase; /* in deg */
    Flt64_LE      ebeamDMP502Charge; /* in Nel */
} EBEAMINFO;

#define EBEAM_INFO_ERROR 0x4000

extern EBEAMINFO    bldEbeamInfo;
extern epicsMutexId bldMutex;
extern IOSCANPVT    bldIoscan;         /* Trigger full-rate EPICS records */

#define BLDMCAST_DST_IP	    "239.255.24.0"
#define BLDMCAST_DST_PORT	10148

#ifdef __cplusplus
}
#endif

#endif
