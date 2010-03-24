/* $Id$ */
#ifndef _BLD_MCAST_H_
#define _BLD_MCAST_H_

#include <epicsTime.h>

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

#endif
