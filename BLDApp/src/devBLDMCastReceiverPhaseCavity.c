/* $Id: devBLDMCastReceiverPhaseCavity.c,v 1.1.2.1 2013/05/24 22:12:50 lpiccoli Exp $ */

#include <string.h>
#include <stdlib.h>

#include <epicsExport.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <alarm.h>
#include <aiRecord.h>

#include "BLDMCast.h"
#include "BLDMCastReceiver.h"
#include "BLDMCastReceiverPhaseCavity.h"

extern int BLD_MCAST_DEBUG;
extern IOSCANPVT bldPhaseCavityIoscan;
extern BLDMCastReceiver *bldPhaseCavityReceiver;
extern pcavPulseId;

/* define function flags */
typedef enum {
  PCAV_CHARGE1,
  PCAV_CHARGE2,
  PCAV_FITTIME1,
  PCAV_FITTIME2,
  PCAV_PULSEID,
  PCAV_STATUS,
} BLDPhaseCavityAttributes;

/*      define parameter check for convinence */
#define CHECK_AIPARM(PARM,VAL)\
        if (!strncmp(pai->inp.value.instio.string,(PARM),strlen((PARM)))) {\
                pai->dpvt=(void *)VAL;\
                return (0);\
        }

static long init_ai( struct aiRecord * pai) {
  pai->dpvt = NULL;

  if (pai->inp.type != INST_IO) {
    recGblRecordError(S_db_badField, (void *)pai, "devAiBLD Init_record, Illegal INP");
    pai->pact=TRUE;
    return (S_db_badField);
  }

  CHECK_AIPARM("PCAV_CHARGE1", PCAV_CHARGE1);
  CHECK_AIPARM("PCAV_CHARGE2", PCAV_CHARGE2);
  CHECK_AIPARM("PCAV_FITTIME1", PCAV_FITTIME1);
  CHECK_AIPARM("PCAV_FITTIME2", PCAV_FITTIME2);
  CHECK_AIPARM("PCAV_PULSEID", PCAV_PULSEID);
  CHECK_AIPARM("PCAV_STATUS", PCAV_STATUS);

  recGblRecordError(S_db_badField, (void *) pai, "devAiBLD Init_record, bad parm");
  pai->pact = TRUE;

  return (S_db_badField);
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt) {
  *iopvt = bldPhaseCavityIoscan;
  return 0;
}

static long read_ai(struct aiRecord *pai) {
  BLDPhaseCavity *pcav = bldPhaseCavityReceiver->bld_payload_bsa;
  BLDHeader *header = bldPhaseCavityReceiver->bld_header_bsa;

  /* Make sure BSA buffers receive data from the same PULSEID */
  if (bldPhaseCavityReceiver->bsa_counter == 0) {
    bldPhaseCavityReceiver->bsa_pulseid = __ld_le32(&(header->fiducialId));
  }
  else {
    if (bldPhaseCavityReceiver->bsa_pulseid != __ld_le32(&(header->fiducialId))) {
      bldPhaseCavityReceiver->bsa_pulseid_mismatch++;
    }
  }


  bldPhaseCavityReceiver->bsa_counter++;

  switch ((int)pai->dpvt) {
  case PCAV_CHARGE1:
    pai->val = __ld_le64(&(pcav->charge1));
    break;
  case PCAV_CHARGE2:
    pai->val = __ld_le64(&(pcav->charge2));
    break;
  case PCAV_FITTIME1:
    pai->val = __ld_le64(&(pcav->fitTime1));
    break;
  case PCAV_FITTIME2:
    pai->val = __ld_le64(&(pcav->fitTime2));
    break;
  case PCAV_PULSEID:
    pai->val = pcavPulseId;/*__ld_le32(&(header->fiducialId));*/
    break;
  case PCAV_STATUS:
    pai->val = __ld_le32(&(header->damage));
    break;
  default:
    pai->val = -1;
    bldPhaseCavityReceiver->bsa_counter--;
    break;
  }

  if(pai->tse == epicsTimeEventDeviceTime) {
    /* do timestamp by device support */
    pai->time.secPastEpoch = __ld_le32(&(header->tv_sec));
    pai->time.nsec         = __ld_le32(&(header->tv_nsec));
  }
  
  if (bldPhaseCavityReceiver->bsa_counter >=
      bldPhaseCavityReceiver->payload_count + 2) {
    bldPhaseCavityReceiver->bsa_counter++;
  }


  pai->udf = FALSE;
  
  return 2;
}

struct bld_phase_cavity_sup_set {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
};

struct bld_phase_cavity_sup_set devAiBldPhaseCavity = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};

#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

epicsExportAddress(dset, devAiBldPhaseCavity);
