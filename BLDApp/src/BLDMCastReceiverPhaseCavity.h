/* $Id: BLDMCastReceiverPhaseCavity.h,v 1.2 2014/02/27 23:53:01 lpiccoli Exp $ */

#ifndef _BLDMCASTRECEIVERPHASECAVITY_H_
#define _BLDMCASTRECEIVERPHASECAVITY_H_

#include "BLDTypes.h"

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

int phase_cavity_create(BLDMCastReceiver **bld_receiver, char *multicast_group);
void phase_cavity_report(void *bld_receiver, int level);

#endif
