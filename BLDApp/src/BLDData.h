/* $Id: BLDData.h,v 1.5 2014/06/26 01:46:49 scondam Exp $ */
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
