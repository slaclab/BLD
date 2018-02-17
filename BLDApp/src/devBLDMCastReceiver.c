/*=============================================================================

  Name: devBLDMCastReceiver.c

  Abs:  BLDMCast Receiver device support for data received from PCD.

  Auth: Luciano Piccoli (lpiccoli)
  Mod:	Shantha Condamoor (scondam)

  Mod:  24-Jun-2014 - S.Condamoor: Replaced mcast-group-specific device support (PCAV/IMB etc.) with generic one.
  		25-Jun-2014 - S.Condamoor: BLD-R2-5-5 - swap back nsec/sec fields per PCD.
		31-Mar-2015 - S.Condamoor: Added support for PhaseCavityTestReceiver
		4-May-2015 - S.Condamoor: Added support for FEEGasDetEnergyReceiver
============================================================================= */
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

#include <errlog.h>

#include "BLDTypes.h"
#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverImb.h"
#include "BLDMCastReceiverPhaseCavity.h"
#include "BLDMCastReceiverGdet.h"
#include "devBLDMCastReceiver.h"

extern int BLD_MCAST_DEBUG;

extern IOSCANPVT bldPhaseCavityIoscan;
extern IOSCANPVT bldHxxUm6Imb01Ioscan;
extern IOSCANPVT bldHxxUm6Imb02Ioscan;
extern IOSCANPVT bldPhaseCavityTestIoscan;
extern IOSCANPVT bldFEEGasDetEnergyIoscan;

extern BLDMCastReceiver *bldPhaseCavityReceiver;
extern BLDMCastReceiver *bldHxxUm6Imb01Receiver;
extern BLDMCastReceiver *bldHxxUm6Imb02Receiver;
extern BLDMCastReceiver *bldPhaseCavityTestReceiver;
extern BLDMCastReceiver *bldFEEGasDetEnergyReceiver;

#define DEBUG_PRINT 1
#ifdef DEBUG_PRINT
int devAiBldRecvrFlag = 2;
#endif

static long init_ai( struct aiRecord * pai)
{
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
  
	  switch (paip->mc_group) {	
		  case PhaseCavity: 
			  paip->receiver = bldPhaseCavityReceiver;
			  break;		
		  case FEEGasDetEnergy: 
			  paip->receiver = bldFEEGasDetEnergyReceiver;
			  break;					  	  
		  case HxxUm6Imb01: 
			  paip->receiver = bldHxxUm6Imb01Receiver;								
			  break;
		  case HxxUm6Imb02: 
  			  paip->receiver = bldHxxUm6Imb02Receiver;																																		
			  break;	
		  case PhaseCavityTest: 
			  paip->receiver = bldPhaseCavityTestReceiver;
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
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt)
{
    IOSCANPVT   pIoScanRcvr = NULL;
    int attr = -1;
    int mc_group = -1;

	int nScanned = sscanf(pai->inp.value.instio.string, "%d %d", &attr, &mc_group);		

    if ( nScanned != 2 )
    {
        errlogPrintf( "devBLDMCastReceiver %s: ai_ioint_info: Invalid instio.string: %s\n",
                     pai->name, pai->inp.value.instio.string );
        pai->pact = TRUE;
        return (S_db_badField);
    }

	/* Do some range checks */
	if (( mc_group < 0 ) || ( mc_group > 255 ))
    {
        errlogPrintf( "devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: mc_group # %i out of range 0..255\n",
                      pai->name, mc_group);
        pai->pact = TRUE;
        return (S_db_badField);
	}

	if ( attr < 0 )
    {
        errlogPrintf( "devBLDMCastReceiver: %s: ai_ioint_info() -- Illegal INP: attr # %i < 0\n",
                    pai->name, attr);
        pai->pact = TRUE;
        return (S_db_badField);
	}

	switch ( mc_group )
    {
        case PhaseCavity:       pIoScanRcvr = bldPhaseCavityIoscan;	break;
        case FEEGasDetEnergy:   pIoScanRcvr = bldFEEGasDetEnergyIoscan;	break;			  
        case HxxUm6Imb01:       pIoScanRcvr = bldHxxUm6Imb01Ioscan;	break;
        case HxxUm6Imb02:       pIoScanRcvr = bldHxxUm6Imb02Ioscan;	break;	
        case PhaseCavityTest:   pIoScanRcvr = bldPhaseCavityTestIoscan;	break;			  		  
    }

    if ( pIoScanRcvr == NULL )
    {
        errlogPrintf(   "devBLDMCastReceiver: %s: ai_ioint_info() - IoScanRcvr not initialized for multicast group %d\n",
                        pai->name, mc_group );
        pai->pact = TRUE;
        return (S_db_badField);
    }

    *iopvt = pIoScanRcvr;
    return 0;
}

static long read_ai(struct aiRecord *pai)
{
  aiBld_t *paip;
  float	v = 0.0;
	
	/* Set this to facilitate easy access to mc_group fields */
	if (pai->dpvt == NULL) {
#ifdef DEBUG_PRINT
      DEBUGPRINT(DP_ERROR, devAiBldRecvrFlag, ("devBLDMCastReceiver: read_ai: %s: dpvt is NULL\n", pai->name));
#endif
      return -1;
	}

	paip = &((aiBldDpvt_t*)pai->dpvt)->bldAttr;	
	
	if ( paip->receiver == NULL ) {
#ifdef DEBUG_PRINT
      DEBUGPRINT(DP_ERROR, devAiBldRecvrFlag, ("devBLDMCastReceiver: read_ai: %s: paip->receiver is NULL\n", pai->name));
#endif
      return -1;
	}
    epicsMutexMustLock(paip->receiver->mutex);
	
	
		BLDHeader *header = (BLDHeader *) paip->receiver->bld_header_bsa;	
	
	    if ((paip->attr >= PULSEID)	&& (paip->attr <= STATUS)) { /* BLD Header data */
			switch (paip->attr) {
				case PULSEID:
					v = (int) __ld_le32(&(header->fiducialId));
      				break;
				case STATUS:
      				v = (int) __ld_le32(&(header->damage));
      				break;				
			}
		}
		else {												/* BLD Payload data */
					
			switch (paip->mc_group) {	
				case PhaseCavity:
				case PhaseCavityTest: {
						BLDPhaseCavity *pcav_payload = (BLDPhaseCavity *) paip->receiver->bld_payload_bsa;	
						switch (paip->attr) {
							case CHARGE1:
    			  				v = __ld_le64(&(pcav_payload->charge1));
    			 				 break;
							case CHARGE2:
    			  				v = __ld_le64(&(pcav_payload->charge2));
    			  				break;
							case FITTIME1:
    			  				v = __ld_le64(&(pcav_payload->fitTime1));
    			  				break;
							case FITTIME2:
    			  				v = __ld_le64(&(pcav_payload->fitTime2));
    			  				break;	
						}
					}
					break;	
				case FEEGasDetEnergy: {
						BLDGdet *gdet_payload = (BLDGdet *) paip->receiver->bld_payload_bsa;	
						switch (paip->attr) {
							case ENRC_11:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_11));
    			 				 break;
							case ENRC_12:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_12));
    			  				break;
							case ENRC_21:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_21));
    			  				break;
							case ENRC_22:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_22));
    			  				break;	
							case ENRC_63:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_63));
    			  				break;
							case ENRC_64:
    			  				v = __ld_le64((Flt64_LE *)&(gdet_payload->f_ENRC_64));
    			  				break;									
						}
					}
					break;						
					
				case HxxUm6Imb01: 			
				case HxxUm6Imb02: {
						BLDImb *imb_payload = (BLDImb *) paip->receiver->bld_payload_bsa;	
						switch (paip->attr) {
							case SUM:
    						  v = __ld_le64(&(imb_payload->sum));
    						  break;
							case XPOS:
    						  v = __ld_le64(&(imb_payload->xpos));
    						  break;
							case YPOS:
    						  v = __ld_le64(&(imb_payload->ypos));
    						  break;
							case CHANNEL10:
    						  v = __ld_le64(&(imb_payload->channel10));
    						  break;
							case CHANNEL11:
    						  v = __ld_le64(&(imb_payload->channel11));
    						  break;
							case CHANNEL12:
    						  v = __ld_le64(&(imb_payload->channel12));
    						  break;
							case CHANNEL13:
    						  v = __ld_le64(&(imb_payload->channel13));
    						  break;			
						}
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
	  /* scondam: PCD swapped nsec/sec fields on all BLDS on 25-Jun-2014 */
    	  pai->time.nsec  = __ld_le32(&(header->tv_nsec));
    	  pai->time.secPastEpoch   = __ld_le32(&(header->tv_sec));
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
