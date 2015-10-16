/* $Id: BLDMCast.h,v 1.34.2.2 2015/10/16 09:16:30 bhill Exp $ */
/*=============================================================================

  Name: BLDMCast.h

  Abs:  BLDMCast driver header for eBeam MCAST BLD data sent to PCDS.

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
		28-Apr-2014 - S.Condamoor -  BLD-R2-5-2, BLD-R2-5-1, BLD-R2-5-0 - no changes affecting this file.
		7-Jul-2014  - S.Condamoor - BLD-R2-6-0 - Added Photon Energy Calculation to eBeam BLD MCAST data . Version 0x6000f
											   - Swapped ts_nsec and ts_sec fields in EBEAMINFO per PCDS (M.Browne) request.
		2-Feb-2015  - S.Condamoor - BLD-R2-6-3 - Fix for etax in Photon Energy Calculation . Version 0x7000f											   
		18-Sep-2015 - B.Hill - Modified for linux compatibility
-----------------------------------------------------------------------------*/
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

/* Shantha Condamoor: 7-Jul-2014 */
/* shot-to-shot Photon Energy Calculation added */
/* Addition of shot-to-shot X position of BPMS:LTU1:[250/450]:X  */
#define EBEAMINFO_VERSION_4 0x6000f

/* Shantha Condamoor: 2-Feb-2015 */
/* Correction for Photon Energy Calculation */
/* Sign-error error was discovered in the calculation of the photon energy that goes into the ebeam bld
   This resulted in negative correlation or negative slope to L3, i.e., lower L3 energies yield higher photon energies, 
   which cannot be the case. The overall magnitude of the photon energy is ok, however.
   
   eVdelta = eVave * ( (x450 - x450ave) - (x250 - x250ave))/ (etax)
   
   In the formula above, etax must be -125 mm instead of +125 mm.
   The dispersion for the first BPM (BPMS:LTU1:250) is positive, the one for the second BPM (BPMS:LTU1:450) is negative,
   So the fix is to define etax=-125 mm

   Increment the ebeam bld version number per PCDS request, so the data is clearly marked as changed
*/
#define EBEAMINFO_VERSION_5 0x7000f

/* Size of original data packet */
#define EBEAMINFO_VERSION_0_SIZE 80

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_1_SIZE (EBEAMINFO_VERSION_0_SIZE + sizeof(Flt64_LE) * 3)

/* Addition of 4 more floats */
#define EBEAMINFO_VERSION_2_SIZE (EBEAMINFO_VERSION_1_SIZE + sizeof(Flt64_LE) * 4)

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_3_SIZE (EBEAMINFO_VERSION_2_SIZE + sizeof(Flt64_LE) * 3)

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_4_SIZE (EBEAMINFO_VERSION_3_SIZE + sizeof(Flt64_LE) * 3)

/* Shantha Condamoor: 2-Feb-2015 
   No change to data structures - same as previous version */
#define EBEAMINFO_VERSION_5_SIZE EBEAMINFO_VERSION_4_SIZE

/**
 * Structure defined in this document:
 * https://confluence.slac.stanford.edu/download/attachments/10256639/bldicd.pdf
 */
typedef struct EBEAMINFO
{
  /* Shantha Condamoor: 7-Jul-2014: Swapped sec and nsec fields per PCDS request (M.Browne and C.O.Grady) */
    Uint32_LE     ts_nsec;   
    Uint32_LE     ts_sec;  
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
	
    /* Added in VERSION_4 */	
	/* Shantha Condamoor: 7-Jul-2014 */
	/* shot-to-shot Photon Energy Calculation added */
	/* Addition of shot-to-shot X position of BPMS:LTU1:[250/450]:X  */	
    Flt64_LE      ebeamPhotonEnergy;/* in eV */	
    Flt64_LE      ebeamLTU250PosX;  /* in mm */  	
    Flt64_LE      ebeamLTU450PosX;  /* in mm */           
	
} EBEAMINFO;

#define EBEAM_INFO_ERROR 0x4000

#define EBEAMCHARGE_DAMAGEMASK			0x1
#define EBEAML3ENERGY_DAMAGEMASK		0x2
#define EBEAMLTUPOSX_DAMAGEMASK			0x4
#define EBEAMLTUPOSY_DAMAGEMASK			0x8
#define EBEAMLTUANGX_DAMAGEMASK			0x10
#define EBEAMLTUANGY_DAMAGEMASK			0x20
#define EBEAMBC2CURRENT_DAMAGEMASK		0x40
#define EBEAMBC2ENERGY_DAMAGEMASK		0x80
#define EBEAMBC1CURRENT_DAMAGEMASK		0x100
#define EBEAMBC1ENERGY_DAMAGEMASK		0x200
#define EBEAMUNDPOSX_DAMAGEMASK			0x400
#define EBEAMUNDPOSY_DAMAGEMASK			0x800
#define EBEAMUNDANGX_DAMAGEMASK			0x1000
#define EBEAMUNDANGY_DAMAGEMASK			0x2000
#define EBEAMXTCAVAMPL_DAMAGEMASK		0x4000
#define EBEAMXTCAVPHASE_DAMAGEMASK		0x8000
#define EBEAMDMP502CHARGE_DAMAGEMASK	0x10000
#define EBEAMPHTONENERGY_DAMAGEMASK		0x20000
#define	EBEAMLTU250POSX_DAMAGEMASK		0x40000
#define EBEAMLTU450POSX_DAMAGEMASK		0x80000

extern EBEAMINFO    bldEbeamInfo;
extern epicsMutexId bldMutex;
extern IOSCANPVT    bldIoscan;         /* Trigger full-rate EPICS records */

/* #define BLDMCAST_DST_IP	    "239.255.24.0" Stubbed so it won't conflict */
#define BLDMCAST_DST_IP	    "239.255.24.254"
#define BLDMCAST_DST_PORT	10148

#ifdef __cplusplus
}
#endif

#endif
