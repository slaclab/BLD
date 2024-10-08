#ifndef _BLDMCASTRECEIVERIMB_H_
#define _BLDMCASTRECEIVERIMB_H_

#include "BLDTypes.h"

#define BLD_IMB_PORT 10148

/*
 * Imb Data
 * Everything that has "Imb" has the following payload structure
 */
#if 0
class BldDataIpimbV1 {
public:
  enum { TypeId = Pds::TypeId::Id_SharedIpimb }; // XTC type ID value (from Pds::TypeId class)
  enum { Version = 1 }; // XTC type version number
  BldDataIpimbV1() {}
  BldDataIpimbV1(const BldDataIpimbV1& other) {
    const char* src = reinterpret_cast<const char*>(&other);
    std::copy(src, src+other._sizeof(), reinterpret_cast<char*>(this));
  }
  BldDataIpimbV1& operator=(const BldDataIpimbV1& other) {
    const char* src = reinterpret_cast<const char*>(&other);
    std::copy(src, src+other._sizeof(), reinterpret_cast<char*>(this));
    return *this;
  }
  const Ipimb::DataV2& ipimbData() const { return _ipimbData; }
  const Ipimb::ConfigV2& ipimbConfig() const { return _ipimbConfig; }
  const Lusi::IpmFexV1& ipmFexData() const { return _ipmFexData; }
  static uint32_t _sizeof() { return ((((((0+(Ipimb::DataV2::_sizeof()))+(Ipimb::ConfigV2::_sizeof()))+(Lusi::IpmFexV1::_sizeof()))+4)-1)/4)*4; }
private:
  Ipimb::DataV2	_ipimbData;
  Ipimb::ConfigV2	_ipimbConfig;
  Lusi::IpmFexV1	_ipmFexData;
};
#endif

typedef struct BLDImb {

  /* IPIMB Raw data */
   Uint32_LE	triggerCounterHigh;
   Uint32_LE	triggerCounterLow;   
   Uint16_LE	config0;
   Uint16_LE	config1;
   Uint16_LE	config2;
   Uint16_LE	channel0;	/**< Raw counts value returned from channel 0. */
   Uint16_LE	channel1;	/**< Raw counts value returned from channel 1. */
   Uint16_LE	channel2;	/**< Raw counts value returned from channel 2. */
   Uint16_LE	channel3;	/**< Raw counts value returned from channel 3. */
   Uint16_LE	channel0ps;	/**< Raw counts value returned from channel 0. */
   Uint16_LE	channel1ps;	/**< Raw counts value returned from channel 1. */
   Uint16_LE	channel2ps;	/**< Raw counts value returned from channel 2. */
   Uint16_LE	channel3ps;	/**< Raw counts value returned from channel 3. */
   Uint16_LE	checksum;

  /* IPIMB Configuration */
   Uint32_LE	triggerCounterConfigHigh;
   Uint32_LE	triggerCounterConfigLow;   
   Uint32_LE	serialIDHigh;
   Uint32_LE	serialIDLow;   
   Uint16_LE	chargeAmpRange;
   Uint16_LE	calibrationRange;
   Uint32_LE	resetLength;
   Uint32_LE	resetDelay;
   Flt64_LE		chargeAmpRefVoltage;
   Flt64_LE		calibrationVoltage;
   Flt64_LE		diodeBias;
   Uint16_LE	status;
   Uint16_LE	errors;
   Uint16_LE	calStrobeLength;
   Uint16_LE	pad0;
   Uint32_LE	trigDelay;
   Uint32_LE	trigPsDelay;
   Uint32_LE	adcDelay;

  /* Processed results */
  Flt64_LE channel10; /* ? */
  Flt64_LE channel11; /* ? */  
  Flt64_LE channel12; /* ? */
  Flt64_LE channel13; /* ? */    
  Flt64_LE sum; /* in ? */
  Flt64_LE xpos; /* in ? */
  Flt64_LE ypos; /* in ? */
  
} BLDImb;

#define BLD_IMB_PARAMS 7

int imb_create(BLDMCastReceiver **bld_receiver,char *multicast_group);
void imb_report(void *bld_receiver, int level);

#endif
