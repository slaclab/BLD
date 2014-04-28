/* $Id: BLDMCast.h,v 1.26 2014/02/28 19:00:53 lpiccoli Exp $ */
#ifndef _BLD_MCAST_H_
#define _BLD_MCAST_H_

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsMutex.h>
#include <dbScan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union Endianness {
	epicsUInt16 test;
	struct {
		epicsUInt8 little, big;
	}           is;
} Endianness;

typedef epicsUInt32 __u32_a __attribute__((may_alias));

static __inline__ __u32_a
__le32(__u32_a v)
{
Endianness   endian = {test:1};

	if ( endian.is.big ) {
		__u32_a m = 0x00ff00ff;

		v = ((v&m)<<8) | ((v>>8)&m);
		v = (v<<16)    | (v>>16);
	}

	return v;
}

static __inline__ __u32_a
__ld_le32(__u32_a *p)
{
__u32_a      v;
#ifdef __PPC__
Endianness   endian = {test:1};

	if ( endian.is.big ) {
		__asm__ __volatile__("lwbrx %0,%y1":"=r"(v):"Z"(*p));
	} else
#endif
	v  = __le32(*p);

	return v;
}

static __inline__ void
__st_le32(__u32_a *p, __u32_a v)
{
#ifdef __PPC__
Endianness endian = {test:1};
	if ( endian.is.big ) {
		__asm__ __volatile__("stwbrx %1,%y0":"=Z"(*p):"r"(v));
	} else
#endif
	*p = __le32(v);
}

typedef epicsUInt16 __u16_a __attribute__((may_alias));

static __inline__ __u16_a
__le16(__u16_a v)
{
Endianness   endian = {test:1};

	if ( endian.is.big ) {
		__u16_a m = 0x0f0f;

		v = ((v&m)<<4) | ((v>>4)&m);
		v = (v<<8)    | (v>>8);
	}

	return v;
}

static __inline__ __u16_a
__ld_le16(__u16_a *p)
{
__u16_a      v;
#ifdef __PPC__
Endianness   endian = {test:1};

	if ( endian.is.big ) {
		__asm__ __volatile__("lwbrx %0,%y1":"=r"(v):"Z"(*p));
	} else
#endif
	v  = __le16(*p);

	return v;
}

static __inline__ void
__st_le16(__u16_a *p, __u16_a v)
{
#ifdef __PPC__
Endianness endian = {test:1};
	if ( endian.is.big ) {
		__asm__ __volatile__("stwbrx %1,%y0":"=Z"(*p):"r"(v));
	} else
#endif
	*p = __le16(v);
}

typedef union Flt64_LE_ {
	__u32_a      u[2];
	epicsFloat64 d;
} Flt64_LE;

typedef __u32_a Uint32_LE;
typedef __u16_a Uint16_LE;

/* This is optimized for PPC where FP registers
 * cannot be manipulated nor copied to integer registers
 * directly but must be written to memory first.
 * Hence, to byte-swap we must write the FP register
 * to memory first, read back into integer registers,
 * swap and write to memory again.
 */

static __inline__ void
__st_le64(Flt64_LE *p, double v)
{
Endianness endian = {test:1};

	p->d = v;
	if ( endian.is.big ) {
		__u32_a tmp;
		tmp = p->u[1];
		__st_le32(p->u+1,p->u[0]);
		__st_le32(p->u,  tmp);
	}
}

static __inline__ double
__ld_le64(Flt64_LE *p)
{
Endianness endian = {test:1};

	if ( endian.is.big ) {
		Flt64_LE tmp;
		__st_le32(tmp.u+1,p->u[0]);
		__st_le32(tmp.u  ,p->u[1]);
		return tmp.d;
	}

	return p->d;
}

/* Original version */
#define EBEAMINFO_VERSION_0 0x1000f

/* Addition of BC2ENERGY, BC1CHARGE and BC1ENERGY */
#define EBEAMINFO_VERSION_1 0x3000f

/* Addition of X, X', Y, Y' calculaded by the Undulator Launch 120Hz feedback */
#define EBEAMINFO_VERSION_2 0x4000f

/* Addition of BPMS:LTU1:[250/450]:X and BPMS:DMP1:502:TMIT */
#define EBEAMINFO_VERSION_3 0x5000f

/* Size of original data packet */
#define EBEAMINFO_VERSION_0_SIZE 80

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_1_SIZE (EBEAMINFO_VERSION_0_SIZE + sizeof(Flt64_LE) * 3)

/* Addition of 4 more floats */
#define EBEAMINFO_VERSION_2_SIZE (EBEAMINFO_VERSION_1_SIZE + sizeof(Flt64_LE) * 4)

/* Addition of 3 more floats */
#define EBEAMINFO_VERSION_3_SIZE (EBEAMINFO_VERSION_2_SIZE + sizeof(Flt64_LE) * 3)

/**
 * Structure defined in this document:
 * https://confluence.slac.stanford.edu/download/attachments/10256639/bldicd.pdf
 */
  typedef struct EBEAMINFO {
    Uint32_LE     ts_sec;
    Uint32_LE     ts_nsec;
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
} EBEAMINFO;

#define EBEAM_INFO_ERROR 0x4000

extern EBEAMINFO    bldEbeamInfo;
extern epicsMutexId bldMutex;
extern IOSCANPVT    bldIoscan;         /* Trigger full-rate EPICS records */

#define BLDMCAST_DST_IP	    "239.255.24.0"
#define BLDMCAST_DST_PORT	10148

#ifdef __cplusplus
}
#endif

#endif
