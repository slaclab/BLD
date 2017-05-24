/* $Id: BLDMCastReceiverPhaseCavity.h,v 1.3 2014/06/25 01:00:22 scondam Exp $ */

#ifndef _BLDMCASTRECEIVERPHASECAVITY_H_
#define _BLDMCASTRECEIVERPHASECAVITY_H_

#include "BLDTypes.h"

#define BLD_PHASE_CAVITY_PORT 10148

/**
 * Phase Cavity Data
 */
#if 0
class BldDataPhaseCavity {
public:
  enum { TypeId = Pds::TypeId::Id_PhaseCavity }; //  XTC type ID value (from Pds::TypeId class)
  enum { Version = 0 }; //  XTC type version number 
  BldDataPhaseCavity(double arg__fFitTime1, double arg__fFitTime2, double arg__fCharge1, double arg__fCharge2)
    : _fFitTime1(arg__fFitTime1), _fFitTime2(arg__fFitTime2), _fCharge1(arg__fCharge1), _fCharge2(arg__fCharge2)
  {
  }
  BldDataPhaseCavity() {}
  // UND:R02:IOC:16:BAT:FitTime1 value in pico-seconds. 
  double fitTime1() const { return _fFitTime1; }
  // UND:R02:IOC:16:BAT:FitTime2 value in pico-seconds. 
  double fitTime2() const { return _fFitTime2; }
  // UND:R02:IOC:16:BAT:Charge1 value in pico-columbs. 
  double charge1() const { return _fCharge1; }
  // UND:R02:IOC:16:BAT:Charge2 value in pico-columbs. 
  double charge2() const { return _fCharge2; }
  static uint32_t _sizeof() { return 32; }
private:
  double	_fFitTime1;	// UND:R02:IOC:16:BAT:FitTime1 value in pico-seconds. 
  double	_fFitTime2;	// UND:R02:IOC:16:BAT:FitTime2 value in pico-seconds. 
  double	_fCharge1;	// UND:R02:IOC:16:BAT:Charge1 value in pico-columbs. 
  double	_fCharge2;	// UND:R02:IOC:16:BAT:Charge2 value in pico-columbs. 
};
#endif

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
