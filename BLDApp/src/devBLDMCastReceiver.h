#ifndef _DEVBLDMCASTRECEIVER_H_
#define _DEVBLDMCASTRECEIVER_H_

#define BLD_PHASE_CAVITY_PORT 10148
#define BLD_IMB_PORT 10148

/* ************************************************************************ */
typedef struct aiBld_s {
  int attr;
  int mc_group;
  BLDMCastReceiver *receiver;
  BLDHeader *header; 
  BLDPhaseCavity *pcav_payload; 
  BLDImb *imb_payload; 
} aiBld_t;

typedef struct aiBldDpvt_s {
	aiBld_t              bldAttr;
} aiBldDpvt_t;

/* ************************************************************************ */
/* define function flags for BSA */

typedef enum {
  PULSEID,			/* 0 */
  STATUS,			/* 1 */	
} BLDHeaderAttributes;

typedef enum {
  CHARGE1=2,		/* 2 */
  CHARGE2,			/* 3 */
  FITTIME1,		/* 4 */
  FITTIME2,		/* 5 */
} BLDPhaseCavityAttributes;

typedef enum {
  	SUM=2,				/* 2 */
  	XPOS,				/* 3 */
  	YPOS,				/* 4 */
  	CHANNEL10,			/* 5 */
  	CHANNEL11,			/* 6 */
  	CHANNEL12,			/* 7 */
  	CHANNEL13,			/* 8 */      
} BLDImbAttributes;

/* ************************************************************************ */

#endif
