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
#include "BLDMCastReceiverPhaseCavity.h"
#include "devBLDMCastReceiver.h"

extern int BLD_MCAST_DEBUG;

extern IOSCANPVT bldPhaseCavityIoscan;
extern IOSCANPVT bldHxxUm6Imb01Ioscan;
extern IOSCANPVT bldHxxUm6Imb02Ioscan;

extern BLDMCastReceiver *bldPhaseCavityReceiver;
extern BLDMCastReceiver *bldHxxUm6Imb01Receiver;
extern BLDMCastReceiver *bldHxxUm6Imb02Receiver;

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
		errlogPrintf("devBLDMCastReceiver: %s: init_record() -- Illegal INP: mc_group # %i out of range (HxxUm6Imb01..CxiDg4Imb01)\n",
		pai->name, paip->mc_group);
		pai->pact = TRUE;
		return (S_db_badField);
	  }
	  
	  if ( paip->attr < 0 ) {
		errlogPrintf("devBLDMCastReceiver: %s: init_record() -- Illegal INP: attr # %i out of range (SUM..STATUS)\n",
		pai->name, paip->attr);
		pai->pact = TRUE;
		return (S_db_badField);
	  }
	  
	  paip->receiver = NULL;
	  paip->header = NULL;
	  paip->pcav_payload = NULL;		  
	  paip->imb_payload = NULL;	  
  
	  switch (paip->mc_group) {	
				case PhaseCavity: 
					paip->receiver = bldPhaseCavityReceiver;
					paip->header = (BLDHeader *) bldPhaseCavityReceiver->bld_header_bsa;	
					paip->pcav_payload = (BLDPhaseCavity *) bldPhaseCavityReceiver->bld_payload_bsa;
					break;			  
				case HxxUm6Imb01: 
					paip->receiver = bldHxxUm6Imb01Receiver;
					paip->header = (BLDHeader *) bldHxxUm6Imb01Receiver->bld_header_bsa;	
					paip->imb_payload = (BLDImb *) bldHxxUm6Imb01Receiver->bld_payload_bsa;					
					break;
				case HxxUm6Imb02: 
  					paip->receiver = bldHxxUm6Imb02Receiver;
    				paip->header = (BLDHeader *) bldHxxUm6Imb02Receiver->bld_header_bsa;	
					paip->imb_payload = (BLDImb *)bldHxxUm6Imb02Receiver->bld_payload_bsa;																																
					break;				  
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
	  errlogPrintf("devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: mc_group # %i out of range (HxxUm6Imb01..CxiDg4Imb01)\n",
	  pai->name, mc_group);
	  pai->pact = TRUE;
	  return (S_db_badField);
	}

	if ( attr < 0 ) {
	  errlogPrintf("devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: attr # %i out of range (SUM..STATUS)\n",
	  pai->name, attr);
	  pai->pact = TRUE;
	  return (S_db_badField);
	}		
	
	switch ( mc_group ) {	
			  case PhaseCavity: *iopvt = bldPhaseCavityIoscan;	break;
			  case HxxUm6Imb01: *iopvt = bldHxxUm6Imb01Ioscan;	break;
			  case HxxUm6Imb02: *iopvt = bldHxxUm6Imb02Ioscan;	break;			  
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
	
    epicsMutexLock(paip->receiver->mutex);
	
	    if ((paip->attr >= PULSEID)	&& (paip->attr <= STATUS)) { /* BLD Header data */
			switch (paip->attr) {
				case PULSEID:
					v = (int) __ld_le32(&(paip->header->fiducialId));
      				break;
				case STATUS:
      				v = (int) __ld_le32(&(paip->header->damage));
      				break;				
			}
		}
		else {												/* BLD Payload data */
			switch (paip->mc_group) {	
				case PhaseCavity: 				
					switch (paip->attr) {
						case CHARGE1:
    			  			v = __ld_le64(&(paip->pcav_payload->charge1));
    			 			 break;
						case CHARGE2:
    			  			v = __ld_le64(&(paip->pcav_payload->charge2));
    			  			break;
						case FITTIME1:
    			  			v = __ld_le64(&(paip->pcav_payload->fitTime1));
    			  			break;
						case FITTIME2:
    			  			v = __ld_le64(&(paip->pcav_payload->fitTime2));
    			  			break;	
					}
					break;	
				case HxxUm6Imb01: 
				case HxxUm6Imb02: 
					switch (paip->attr) {
						case SUM:
    					  v = __ld_le64(&(paip->imb_payload->sum));
    					  break;
						case XPOS:
    					  v = __ld_le64(&(paip->imb_payload->xpos));
    					  break;
						case YPOS:
    					  v = __ld_le64(&(paip->imb_payload->ypos));
    					  break;
						case CHANNEL10:
    					  v = __ld_le64(&(paip->imb_payload->channel10));
    					  break;
						case CHANNEL11:
    					  v = __ld_le64(&(paip->imb_payload->channel11));
    					  break;
						case CHANNEL12:
    					  v = __ld_le64(&(paip->imb_payload->channel12));
    					  break;
						case CHANNEL13:
    					  v = __ld_le64(&(paip->imb_payload->channel13));
    					  break;			
					}
					break;
			}
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
	
    epicsMutexUnlock(paip->receiver->mutex);		

	#ifdef DEBUG_PRINT
	DEBUGPRINT(DP_INFO, devAiBldRecvrFlag,
      ("devBLDMCastReceiver: read_ai: %s: stn: %d has rval: %d gets val: %g\n",
      pai->name, paip->mc_group, pai->rval, pai->val));
	#endif
	pai->udf = FALSE;
	return 2; /* DON'T CONVERT */
}

struct bld_sup_set {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
};

struct bld_sup_set devAiBldRcvr = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};

#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(dset, devAiBldRcvr);
