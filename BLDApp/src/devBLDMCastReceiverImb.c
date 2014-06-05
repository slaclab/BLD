#include <string.h>
#include <stdlib.h>

#include <epicsExport.h>	/* for epicsExportAddress */
#include <dbScan.h>	 		/* for SCAN_IO_EVENT */
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <alarm.h>			/* for INVALID_ALARM, TIMEOUT_ALARM etc. */
#include <aiRecord.h>		/* for aiRecord structure */
#include <debugPrint.h> 	/* for DEBUGPRINT */
#include <devLib.h>			/* for S_dev_noMemory */

#include "BLDTypes.h"
#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverImb.h"

extern int BLD_MCAST_DEBUG;

extern IOSCANPVT bldHxxUm6Imb01Ioscan;
extern IOSCANPVT bldHxxUm6Imb02Ioscan;

extern BLDMCastReceiver *bldHxxUm6Imb01Receiver;
extern BLDMCastReceiver *bldHxxUm6Imb02Receiver;

#ifdef SIGNAL_TEST
extern imbPulseId;
#endif

/* define function flags */
typedef enum {
  	IMB_SUM,
  	IMB_XPOS,
  	IMB_YPOS,
  	IMB_CHANNEL10,
  	IMB_CHANNEL11,
  	IMB_CHANNEL12,
  	IMB_CHANNEL13,      
  	IMB_PULSEID,
  	IMB_STATUS,
} BLDImbAttributes;

typedef enum {
/*XRT */
	IMB_HxxUm6Imb01,		/* 0: "239.255.24.4" */
	IMB_HxxUm6Imb02,		/* 1: "239.255.24.5" */
	IMB_HfxDg2Imb01,		/* 2: "239.255.24.6" */
	IMB_HfxDg2Imb02,		/* 3: "239.255.24.7" */
	IMB_XcsDg3Imb03,		/* 4: "239.255.24.8" */
	IMB_XcsDg3Imb04,		/* 5: "239.255.24.9" */
	IMB_HfxDg3Imb01,		/* 6: "239.255.24.10" */
	IMB_HfxDg3Imb02,		/* 7: "239.255.24.11" */
	IMB_HfxMonImb01,		/* 8: "239.255.24.17" */
	IMB_HfxMonImb02,		/* 9: "239.255.24.18" */
	IMB_HfxMonImb03,		/* 10: "239.255.24.19" */
/* CXI Local */
	IMB_CxiDg1Imb01,		/* 11: "239.255.24.27" */
	IMB_CxiDg2Imb01,		/* 12: "239.255.24.28" */
	IMB_CxiDg2Imb02,		/* 13: "239.255.24.29" */
	IMB_CxiDg4Imb01,		/* 14: "239.255.24.30" */
} BLDImbMCGroup;

typedef struct aiBld_s {
  int attr;
  int mc_group;
  BLDMCastReceiver *receiver;
  BLDImb *imb;
  BLDHeader *header;  
} aiBld_t;

typedef struct aiBldDpvt_s {
	aiBld_t              bldAttr;
} aiBldDpvt_t;

#ifdef DEBUG_PRINT
int devAiBldRecvrFlag = 2;
#endif

static long init_ai( struct aiRecord * pai) {
  aiBld_t *paip;
  aiBldDpvt_t *dpvt;
  
  /* Allocate the memory for this record's private data structure */
  dpvt = malloc(sizeof(*dpvt));
  if (dpvt == NULL) {
    printf("devBLDMCastReceiver: init_ai: %s: out of memory\n", pai->name);
    return S_dev_noMemory;
  }

  paip = &dpvt->bldAttr;
  
  /* initialize attr and mc_group to null values */
  paip->attr = -1;
  paip->mc_group = -1;

  /* Tell the record about its private data structure */
  pai->dpvt = dpvt;

  switch(pai->inp.type) {
    case(INST_IO):
	
      sscanf(pai->inp.value.instio.string, "%d %d", &(paip->attr), &(paip->mc_group));		
			
      #ifdef DEBUG_PRINT
      DEBUGPRINT(DP_INFO, devAiBldRecvrFlag, ("devBLDMCastReceiver: init_ai: %s: init attr with attr %d mc_group %d\n",
      pai->name, paip->attr, paip->mc_group));
      #endif

	  /* Do some range checks */
	  if (( paip->mc_group < 0 ) || (paip->mc_group > 255)) {
		errlogPrintf("devBLDMCastReceiver: %s: init_record() -- Illegal INP: mc_group # %i out of range (IMB_HxxUm6Imb01..IMB_CxiDg4Imb01)\n",
		pai->name, paip->mc_group);
		pai->pact = TRUE;
		return (S_db_badField);
	  }
	  
	  if ( paip->attr < 0 ) {
		errlogPrintf("devBLDMCastReceiver: %s: init_record() -- Illegal INP: attr # %i out of range (IMB_SUM..IMB_STATUS)\n",
		pai->name, paip->attr);
		pai->pact = TRUE;
		return (S_db_badField);
	  }
	  
	  paip->receiver = NULL;
	  paip->imb = NULL;
	  paip->header = NULL;
  
	  switch (paip->mc_group) {	
				case IMB_HxxUm6Imb01: 
					paip->receiver = bldHxxUm6Imb01Receiver;
					paip->imb = bldHxxUm6Imb01Receiver->bld_payload_bsa;
					paip->header = bldHxxUm6Imb01Receiver->bld_header_bsa;	
					break;
				case IMB_HxxUm6Imb02: 
  					paip->receiver = bldHxxUm6Imb02Receiver;
    				paip->imb = bldHxxUm6Imb02Receiver->bld_payload_bsa;
    				paip->header = bldHxxUm6Imb02Receiver->bld_header_bsa;																											
					break;
				case IMB_HfxDg2Imb01:
				case IMB_HfxDg2Imb02:
				case IMB_XcsDg3Imb03:					
				case IMB_XcsDg3Imb04:	
				case IMB_HfxDg3Imb01:	
				case IMB_HfxDg3Imb02:			
				case IMB_HfxMonImb01:		
				case IMB_HfxMonImb02:	
				case IMB_HfxMonImb03:				
				case IMB_CxiDg1Imb01:		
				case IMB_CxiDg2Imb01:		
				case IMB_CxiDg2Imb02:		
				case IMB_CxiDg4Imb01: break;					  
	  }

	  break;

    default:
      #ifdef DEBUG_PRINT
      DEBUGPRINT(DP_ERROR, devAiBldRecvrFlag, ("devBLDMCastReceiver: init_ai: %s: unrecognized link type\n",
        pai->name));
      #endif
	  pai->pact = TRUE;
      return (S_db_badField);
    }

	return 0;
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt) {

  int attr = -1;
  int mc_group = -1;
	
    sscanf(pai->inp.value.instio.string, "%d %d", &attr, &mc_group);		

	/* Do some range checks */
	if (( mc_group < 0 ) || ( mc_group > 255 )) {
	  errlogPrintf("devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: mc_group # %i out of range (IMB_HxxUm6Imb01..IMB_CxiDg4Imb01)\n",
	  pai->name, mc_group);
	  pai->pact = TRUE;
	  return (S_db_badField);
	}

	if ( attr < 0 ) {
	  errlogPrintf("devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: attr # %i out of range (IMB_SUM..IMB_STATUS)\n",
	  pai->name, attr);
	  pai->pact = TRUE;
	  return (S_db_badField);
	}		
	
	switch ( mc_group ) {	
			  case IMB_HxxUm6Imb01: *iopvt = bldHxxUm6Imb01Ioscan;	break;
			  case IMB_HxxUm6Imb02: *iopvt = bldHxxUm6Imb02Ioscan;	break;
			  case IMB_HfxDg2Imb01:
			  case IMB_HfxDg2Imb02:
			  case IMB_XcsDg3Imb03:					
			  case IMB_XcsDg3Imb04:	
			  case IMB_HfxDg3Imb01:	
			  case IMB_HfxDg3Imb02:			
			  case IMB_HfxMonImb01:		
			  case IMB_HfxMonImb02:	
			  case IMB_HfxMonImb03:				
			  case IMB_CxiDg1Imb01:		
			  case IMB_CxiDg2Imb01:		
			  case IMB_CxiDg2Imb02:		
			  case IMB_CxiDg4Imb01: break;					  
	}		
		
  	return 0;
}

static long read_ai(struct aiRecord *pai) {

  aiBld_t *paip;
  float	v;	
	
	/* Set this to facilitate easy access to mc_group fields */
	if (pai->dpvt == NULL) {
      #ifdef DEBUG_PRINT
      DEBUGPRINT(DP_ERROR, devAiBldRecvrFlag, ("devBLDMCastReceiver: read_ai: %s: dpvt is NULL\n", pai->name));
      #endif
      return -1;
	}

	paip = &((aiBldDpvt_t*)pai->dpvt)->bldAttr;	
	
	/* Make sure BSA buffers receive data from the same PULSEID */
	if (paip->receiver->bsa_counter == 0) {
      paip->receiver->bsa_pulseid = __ld_le32(&(paip->header->fiducialId));
	}
	else {
      if (paip->receiver->bsa_pulseid != __ld_le32(&(paip->header->fiducialId))) {
    	paip->receiver->bsa_pulseid_mismatch++;
      }
	}

	paip->receiver->bsa_counter++;

	switch (paip->attr) {
	case IMB_SUM:
      v = __ld_le64(&(paip->imb->sum));
      break;
	case IMB_XPOS:
      v = __ld_le64(&(paip->imb->xpos));
      break;
	case IMB_YPOS:
      v = __ld_le64(&(paip->imb->ypos));
      break;
	case IMB_CHANNEL10:
      v = __ld_le64(&(paip->imb->channel10));
      break;
	case IMB_CHANNEL11:
      v = __ld_le64(&(paip->imb->channel11));
      break;
	case IMB_CHANNEL12:
      v = __ld_le64(&(paip->imb->channel12));
      break;
	case IMB_CHANNEL13:
      v = __ld_le64(&(paip->imb->channel13));
      break;			
	case IMB_PULSEID:
      v = (int) __ld_le32(&(paip->header->fiducialId));
      break;
	case IMB_STATUS:
      v = (int) __ld_le32(&(paip->header->damage));
      break;
	default:
      pai->val = -1;
      paip->receiver->bsa_counter--;
      break;
	}

    /* test for NAN */

    if ( v != v ) {
       /* if val is NaN then there was a timeout */
       recGblSetSevr( pai, TIMEOUT_ALARM, INVALID_ALARM );
    } else {
       pai->val =  v;
	}
	
	if(pai->tse == epicsTimeEventDeviceTime) {
      /* do timestamp by device support */
      pai->time.secPastEpoch = __ld_le32(&(paip->header->tv_sec));
      pai->time.nsec         = __ld_le32(&(paip->header->tv_nsec));
	}
	
	if (paip->receiver->bsa_counter >= paip->receiver->payload_count + 2) {
	 paip->receiver->bsa_counter = 0;
	}

	#ifdef DEBUG_PRINT
	DEBUGPRINT(DP_INFO, devAiBldRecvrFlag,
      ("devBLDMCastReceiver: read_ai: %s: stn: %d has rval: %d gets val: %g\n",
      pai->name, paip->mc_group, pai->rval, pai->val));
	#endif
	pai->udf = FALSE;
	return 2; /* DON'T CONVERT */
}

struct bld_imb_sup_set {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
};

struct bld_imb_sup_set devAiBldImb = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};

#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(dset, devAiBldImb);
