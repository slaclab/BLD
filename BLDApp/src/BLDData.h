/* $Id: BLDData.h,v 1.6 2014/07/08 18:09:10 scondam Exp $ */
/*=============================================================================

  Name: BLDData.h

  Abs:  BLD Data header definition for all MCAST BLD data sent to or received from PCD.

  Auth: Sheng Peng (pengs)
  Mod:	Till Straumann (strauman)
  		Luciano Piccoli (lpiccoli)
  		Shantha Condamoor (scondam)

  Mod:  22-Sep-2009 - S.Peng - Initial Release
		18-May-2010 - T.Straumann: BLD-R2-0-0-BR - Cleanup and modifications
		12-May-2011 - L.Piccoli	- Modifications
		11-Jun-2013 - L.Piccoli	- BLD-R2-2-0 - 	Addition of BLD receiver - phase cavity  
		30-Sep-2013 - L.Piccoli - BLD-R2-3-0 - Addition of Fast Undulator Launch feedback states, version 0x4000f
		28-Feb-2014 - L.Piccoli - BLD-R2-4-0 - Merged BLD-R2-0-0-BR branch with MAIN_TRUNK. Addition of TCAV/DMP1 PVs to BLD. Version 0x5000f
		25-Jun-2014 - S.Condamoor - BLD-R2-5-0 - Swapped ts_nsec and ts_sec fields in BLDHeader per PCD (M.Browne) request.
		4-May-2015 - S.Condamoor: - BLD-R2-6-5 - Added support for FEEGasDetEnergyReceiver			
-----------------------------------------------------------------------------*/
#ifndef _BLDDATA_H_
#define _BLDDATA_H_

#include "BLDTypes.h"

/**
 * BLDData.h
 *
 * Definition of BLD data packets (header and payloads)
 */

/**
 * BLD Header
 */
/**
 * Structure defined in this document:
 * https://confluence.slac.stanford.edu/download/attachments/10256639/bldicd.pdf
class BldPacketHeader
{
public:
    // 
     * Memory Mapped Data
     
#if 1
    uint32_t uNanoSecs;
    uint32_t uSecs;
    uint32_t uMBZ1;
    uint32_t uFiducialId;
    uint32_t uMBZ2;
#else
	// Per Matt weaver 11/20/2014
    uint32_t uClockLow;		// nanoseconds
    uint32_t uClockHigh;	// seconds
    uint32_t uStampLow;		// 119MHz ticks
    uint32_t uStampHigh;	// Fiducial
    uint32_t uEnv;
#endif

    // Xtc Section 1
    uint32_t uDamage;
    uint32_t uLogicalId;
    uint32_t uPhysicalId;
    uint32_t uDataType;
    uint32_t uExtentSize;

    // Xtc Section 2
    uint32_t uDamage2;
    uint32_t uLogicalId2;
    uint32_t uPhysicalId2;
    uint32_t uDataType2;
    uint32_t uExtentSize2;
    
    //
     * Enums imported from PDS repository
     
     
    // Imported from PDS repository: pdsdata/xtc/BldInfo.hh : BldInfo::Type
	// /reg/g/pcds/dist/pds/current/build/pdsdata/include/pdsdata/xtc/BldInfo.hh 
    // Note that this is only the ones *we* handle!  Max is set large for the future!
    enum BldTypeId	{	EBeam, PhaseCavity, FEEGasDetEnergy,
						Nh2Sb1Ipm01,  
						HxxUm6Imb01, HxxUm6Imb02,
						HfxDg2Imb01, HfxDg2Imb02,
						XcsDg3Imb03, XcsDg3Imb04,
						HfxDg3Imb01, HfxDg3Imb02,
						HxxDg1Cam,   HfxDg2Cam,
						HfxDg3Cam,   XcsDg3Cam,
						HfxMonCam,
						HfxMonImb01, HfxMonImb02,
						HfxMonImb03,
						MecLasEm01, MecTctrPip01,
						MecTcTrDio01,
						MecXt2Ipm02, MecXt2Ipm03, 
						MecHxmIpm01,
						GMD,
						NumberOfBldTypeId=100 }; 
    //
       EBeam bld does not use this module
     
 
    // Imported from PDS repository: pdsdata/xtc/TypeId.hh : TypId::Type
	// /reg/g/pcds/dist/pds/current/build/pdsdata/include/pdsdata/xtc/TypeId.hh 
    // Note that this is only the ones *we* handle!  Max is set large for the future!
    enum XtcDataType 
    {
        Any                 = 0, 
        Id_Xtc              = 1,          // generic hierarchical container
        Id_Frame            = 2,        // raw image
        Id_AcqWaveform      = 3,
        Id_AcqConfig        = 4,
        Id_TwoDGaussian     = 5, // 2-D Gaussian + covariances
        Id_Opal1kConfig     = 6,
        Id_FrameFexConfig   = 7,
        Id_EvrConfig        = 8,
        Id_TM6740Config     = 9,
        Id_ControlConfig    = 10,
        Id_pnCCDframe       = 11,
        Id_pnCCDconfig      = 12,
        Id_Epics            = 13,        // Epics Data Type
        Id_FEEGasDetEnergy  = 14,
        Id_EBeam            = 15,
        Id_PhaseCavity      = 16,
		Id_PrincetonFrame	= 17,
		Id_PrincetonConfig	= 18,
		Id_EvrData			= 19,
		Id_FrameFccdConfig	= 20,
		Id_FccdConfig		= 21,
		Id_IpimbData		= 22,
		Id_IpimbConfig		= 23,
		Id_EncoderData		= 24,
		Id_EncoderConfig	= 25,
		Id_EvrIOConfig		= 26,
		Id_PrincetonInfo	= 27,
		Id_CspadElement		= 28,
		Id_CspadConfig		= 29,
		Id_IpmFexConfig		= 30,  // LUSI Diagnostics
		Id_IpmFex			= 31,
		Id_DiodeFexConfig	= 32,
		Id_DiodeFex			= 33,
		Id_PimImageConfig	= 34,
		Id_SharedIpimb		= 35,
		Id_AcqTdcConfig		= 36,
		Id_AcqTdcData		= 37,
		Id_Index			= 38,
		Id_XampsConfig		= 39,
		Id_XampsElement		= 40,
		Id_Cspad2x2Element	= 41,
		Id_SharedPim		= 42,
		Id_Cspad2x2Config	= 43,
		Id_FexampConfig		= 44,
		Id_FexampElement	= 45,
		Id_Gsc16aiConfig	= 46,
		Id_Gsc16aiData		= 47,
		Id_PhasicsConfig	= 48,
		Id_TimepixConfig	= 49,
		Id_TimepixData		= 50,
		Id_CspadCompressedElement	= 51,
		Id_OceanOpticsConfig	= 52,
		Id_OceanOpticsData	= 53,
		Id_EpicsConfig		= 54,
		Id_FliConfig		= 55,
		Id_FliFrame			= 56,
		Id_QuartzConfig		= 57,
		Reserved1			= 58,	// previously Id_CompressedFrame        : no corresponding class
		Reserved2			= 59,	// previously Id_CompressedTimePixFrame : no corresponding class
		Id_AndorConfig		= 60,
		Id_AndorFrame		= 61,
		Id_UsdUsbData		= 62,
		Id_UsdUsbConfig		= 63,
		Id_GMD				= 64,
        NumberOfXtcDataType	= 200
    }; 
*/
 
typedef struct BLDHeader {
  /* scondam: 25-Jun-2014: PCD swapped the tv_nsec and tv_sec fields for several BLDs on this PAMM day */
  /*                       BldSender and BldRcvr apps share this file */

  Uint32_LE tv_nsec;  
  Uint32_LE tv_sec;    
  Uint32_LE mbz1; /* must be zero */
  Uint32_LE fiducialId;
  Uint32_LE mbz2; /* must be zero */
  
  /* Xtc Section 1 */
  Uint32_LE damage;
  Uint32_LE logicalId; /* source 1 */
  Uint32_LE physicalId; /* source 2 */
  Uint32_LE dataType; /* Contains - this is the version field */
  Uint32_LE extentSize; /* Extent - size of data following Xtc Section 2 */
  
  /* Xtc Section 2 */
  Uint32_LE damage2;
  Uint32_LE logicalId2;
  Uint32_LE physicalId2;
  Uint32_LE dataType2;
  Uint32_LE extentSize2; /* Extent - size of data following Xtc Section 2 */
} BLDHeader;

#endif
