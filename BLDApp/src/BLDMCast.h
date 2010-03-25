/* $Id: BLDMCast.h,v 1.16 2010/03/24 00:31:26 strauman Exp $ */
#ifndef _BLD_MCAST_H_
#define _BLD_MCAST_H_

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsMutex.h>
#include <dbScan.h>

typedef struct EBEAMINFO
{
    epicsTimeStamp	timestamp;
    epicsUInt32     uMBZ1;
    epicsUInt32     uFiducialId;
    epicsUInt32     uMBZ2;

    /* Xtc Section 1 */
    epicsUInt32     uDamage;
    epicsUInt32     uLogicalId; /* source 1 */
    epicsUInt32     uPhysicalId;/* source 2 */
    epicsUInt32     uDataType;  /* Contains */
    epicsUInt32     uExtentSize;/* Extent */

    /* Xtc Section 2 */
    epicsUInt32     uDamage2;
    epicsUInt32     uLogicalId2;
    epicsUInt32     uPhysicalId2;
    epicsUInt32     uDataType2;
    epicsUInt32     uExtentSize2;

    epicsUInt32     uDamageMask;

    epicsFloat64	ebeamCharge;	/* in nC */
    epicsFloat64	ebeamL3Energy;	/* in MeV */
    epicsFloat64	ebeamLTUPosX;	/* in mm */
    epicsFloat64	ebeamLTUPosY;	/* in mm */
    epicsFloat64	ebeamLTUAngX;	/* in mrad */
    epicsFloat64	ebeamLTUAngY;	/* in mrad */

    epicsFloat64	ebeamBunchLen;	/* in Amps */
} EBEAMINFO;

#define EBEAM_INFO_ERROR 0x4000

extern EBEAMINFO    ebeamInfo;
extern epicsMutexId mutexLock;
extern IOSCANPVT    ioscan;         /* Trigger EPICS record */

#endif
