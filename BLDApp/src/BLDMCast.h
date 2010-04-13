/* $Id: BLDMCast.h,v 1.20 2010/04/08 22:00:17 strauman Exp $ */
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

typedef union Flt64_LE_ {
	__u32_a      u[2];
	epicsFloat64 d;
} Flt64_LE;

typedef __u32_a Uint32_LE;

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
		__u32_a tmp;
		tmp = p->u[1];
		__st_le32(p->u+1,p->u[0]);
		__st_le32(p->u,  tmp);
	}

	return p->d;
}


typedef struct EBEAMINFO
{
	Uint32_LE     ts_sec;
	Uint32_LE     ts_nsec;
    Uint32_LE     uMBZ1;
    Uint32_LE     uFiducialId;
    Uint32_LE     uMBZ2;

    /* Xtc Section 1 */
    Uint32_LE     uDamage;
    Uint32_LE     uLogicalId;   /* source 1 */
    Uint32_LE     uPhysicalId;  /* source 2 */
    Uint32_LE     uDataType;    /* Contains */
    Uint32_LE     uExtentSize;  /* Extent */

    /* Xtc Section 2 */
    Uint32_LE     uDamage2;
    Uint32_LE     uLogicalId2;
    Uint32_LE     uPhysicalId2;
    Uint32_LE     uDataType2;
    Uint32_LE     uExtentSize2;

    Uint32_LE     uDamageMask;

    Flt64_LE      ebeamCharge;   /* in nC */
    Flt64_LE      ebeamL3Energy; /* in MeV */
    Flt64_LE      ebeamLTUPosX;  /* in mm */
    Flt64_LE      ebeamLTUPosY;  /* in mm */
    Flt64_LE      ebeamLTUAngX;  /* in mrad */
    Flt64_LE      ebeamLTUAngY;  /* in mrad */

    Flt64_LE      ebeamBunchLen; /* in Amps */
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
