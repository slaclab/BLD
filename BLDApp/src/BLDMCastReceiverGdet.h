#ifndef _BLDMCASTRECEIVERGDET_H_
#define _BLDMCASTRECEIVERGDET_H_

#include "BLDTypes.h"

#define BLD_GDET_PORT 10148

/**
 *  FEEGasDetEnergyV1 Data
   Everything that has "Gdet" has the following payload structure
class BldDataFEEGasDetEnergyV1 {
public:
  enum { TypeId = Pds::TypeId::Id_FEEGasDetEnergy //< XTC type ID value (from Pds::TypeId class)  };
  enum { Version = 1 //< XTC type version number  };
  BldDataFEEGasDetEnergyV1(double arg__f_11_ENRC, double arg__f_12_ENRC, double arg__f_21_ENRC, double arg__f_22_ENRC, double arg__f_63_ENRC, double arg__f_64_ENRC)
    : _f_11_ENRC(arg__f_11_ENRC), _f_12_ENRC(arg__f_12_ENRC), _f_21_ENRC(arg__f_21_ENRC), _f_22_ENRC(arg__f_22_ENRC), _f_63_ENRC(arg__f_63_ENRC), _f_64_ENRC(arg__f_64_ENRC)
  {
  }
  BldDataFEEGasDetEnergyV1() {}
  // First energy measurement (mJ) before attenuation. (pv name GDET:FEE1:241:ENRC) 
  double f_11_ENRC() const { return _f_11_ENRC; }
  // Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:242:ENRC) 
  double f_12_ENRC() const { return _f_12_ENRC; }
  // First energy measurement (mJ) after attenuation. (pv name  GDET:FEE1:361:ENRC) 
  double f_21_ENRC() const { return _f_21_ENRC; }
  // Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:362:ENRC) 
  double f_22_ENRC() const { return _f_22_ENRC; }
  // First energy measurement (mJ) for small signals (<0.5 mJ), after attenuation. (pv name GDET:FEE1:363:ENRC) 
  double f_63_ENRC() const { return _f_63_ENRC; }
  // Second (duplicate!) energy measurement (mJ) for small signals (<0.5mJ), after attenutation. (pv name GDET:FEE1:364:ENRC) 
  double f_64_ENRC() const { return _f_64_ENRC; }
  static uint32_t _sizeof() { return 48; }
private:
  double	_f_11_ENRC;	//< First energy measurement (mJ) before attenuation. (pv name GDET:FEE1:241:ENRC) 
  double	_f_12_ENRC;	//< Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:242:ENRC) 
  double	_f_21_ENRC;	//< First energy measurement (mJ) after attenuation. (pv name  GDET:FEE1:361:ENRC) 
  double	_f_22_ENRC;	//< Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:362:ENRC) 
  double	_f_63_ENRC;	//< First energy measurement (mJ) for small signals (<0.5 mJ), after attenuation. (pv name GDET:FEE1:363:ENRC) 
  double	_f_64_ENRC;	//< Second (duplicate!) energy measurement (mJ) for small signals (<0.5mJ), after attenutation. (pv name GDET:FEE1:364:ENRC) 
};   
*/

typedef struct BLDGdet {

  /* GDET processed data */
  double	f_ENRC_11;	/**< First energy measurement (mJ) before attenuation. (pv name GDET:FEE1:241:ENRC) */
  double	f_ENRC_12;	/**< Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:242:ENRC) */
  double	f_ENRC_21;	/**< First energy measurement (mJ) after attenuation. (pv name  GDET:FEE1:361:ENRC) */
  double	f_ENRC_22;	/**< Second (duplicate!) energy measurement (mJ) after attenuation. (pv name GDET:FEE1:362:ENRC) */
  double	f_ENRC_63;	/**< First energy measurement (mJ) for small signals (<0.5 mJ), after attenuation. (pv name GDET:FEE1:363:ENRC) */
  double	f_ENRC_64;	/**< Second (duplicate!) energy measurement (mJ) for small signals (<0.5mJ), after attenutation. (pv name GDET:FEE1:364:ENRC) */  
} BLDGdet;

#define BLD_GDET_PARAMS 6

int gdet_create(BLDMCastReceiver **bld_receiver,char *multicast_group);
void gdet_report(void *bld_receiver, int level);

#endif
