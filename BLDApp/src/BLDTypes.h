#ifndef _BLDTYPES_H_
#define _BLDTYPES_H_

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

#ifdef __cplusplus
}
#endif

#endif

