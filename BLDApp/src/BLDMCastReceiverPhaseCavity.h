/* $Id: $ */

#ifndef _BLDMCASTRECEIVERPHASECAVITY_H_
#define _BLDMCASTRECEIVERPHASECAVITY_H_

#include "BLDMCast.h"

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
