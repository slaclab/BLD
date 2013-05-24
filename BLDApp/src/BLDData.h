/* $Id: $ */

#ifndef _BLDDATA_H_
#define _BLDDATA_H_

#include "BLDMCast.h"

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
  Uint32_LE tv_sec;
  Uint32_LE tv_nsec;
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
