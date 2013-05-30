/* $Id: BLDMCastReceiverPhaseCavity.h,v 1.1.2.1 2013/05/24 22:12:06 lpiccoli Exp $ */

#ifndef _BLDMCASTRECEIVERPHASECAVITY_H_
#define _BLDMCASTRECEIVERPHASECAVITY_H_

#include "BLDMCast.h"

#define BLD_PHASE_CAVITY_GROUP "239.255.24.1"
#define BLD_PHASE_CAVITY_PORT 10148

/**
 * Phase Cavity Data
 */
typedef struct BLDPhaseCavity {
  Flt64_LE fitTime1; /* in pico-seconds */
  Flt64_LE fitTime2; /* in pico-seconds */
  Flt64_LE charge1; /* in pico-columbs */
  Flt64_LE charge2; /* in pico-columbs */
} BLDPhaseCavity;

#define BLD_PHASE_CAVITY_PARAMS 4

int phase_cavity_create(BLDMCastReceiver **bld_receiver);
void phase_cavity_report(void *bld_receiver, int level);
void phase_cavity_run(void *bld_receiver);

#endif
