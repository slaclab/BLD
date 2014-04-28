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
#include "BLDMCastReceiverImb.h"

extern int BLD_MCAST_DEBUG;
extern IOSCANPVT bldImbIoscan;
extern BLDMCastReceiver *bldImbReceiver;

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

  CHECK_AIPARM("IMB_SUM", IMB_SUM);
  CHECK_AIPARM("IMB_XPOS", IMB_XPOS);
  CHECK_AIPARM("IMB_YPOS", IMB_YPOS);
  CHECK_AIPARM("IMB_CHANNEL10", IMB_CHANNEL10);
  CHECK_AIPARM("IMB_CHANNEL11", IMB_CHANNEL11);
  CHECK_AIPARM("IMB_CHANNEL12", IMB_CHANNEL12);
  CHECK_AIPARM("IMB_CHANNEL13", IMB_CHANNEL13);      
  CHECK_AIPARM("IMB_PULSEID", IMB_PULSEID);
  CHECK_AIPARM("IMB_STATUS", IMB_STATUS);

  recGblRecordError(S_db_badField, (void *) pai, "devAiBLD Init_record, bad parm");
  pai->pact = TRUE;

  return (S_db_badField);
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt) {
  *iopvt = bldImbIoscan;
  return 0;
}

static long read_ai(struct aiRecord *pai) {
  BLDImb *imb = bldImbReceiver->bld_payload_bsa;
  BLDHeader *header = bldImbReceiver->bld_header_bsa;

  /* Make sure BSA buffers receive data from the same PULSEID */
  if (bldImbReceiver->bsa_counter == 0) {
    bldImbReceiver->bsa_pulseid = __ld_le32(&(header->fiducialId));
  }
  else {
    if (bldImbReceiver->bsa_pulseid != __ld_le32(&(header->fiducialId))) {
      bldImbReceiver->bsa_pulseid_mismatch++;
    }
  }


  bldImbReceiver->bsa_counter++;

  switch ((int)pai->dpvt) {
  case IMB_SUM:
    pai->val = __ld_le64(&(imb->sum));
    break;
  case IMB_XPOS:
    pai->val = __ld_le64(&(imb->xpos));
    break;
  case IMB_YPOS:
    pai->val = __ld_le64(&(imb->ypos));
    break;
  case IMB_CHANNEL10:
    pai->val = __ld_le64(&(imb->channel10));
    break;
  case IMB_CHANNEL11:
    pai->val = __ld_le64(&(imb->channel11));
    break;
  case IMB_CHANNEL12:
    pai->val = __ld_le64(&(imb->channel12));
    break;
  case IMB_CHANNEL13:
    pai->val = __ld_le64(&(imb->channel13));
    break;			
  case IMB_PULSEID:
#ifdef SIGNAL_TEST
    pai->val = imbPulseId;
#else
    pai->val = __ld_le32(&(header->fiducialId));
#endif
    break;
  case IMB_STATUS:
    pai->val = __ld_le32(&(header->damage));
    break;
  default:
    pai->val = -1;
    bldImbReceiver->bsa_counter--;
    break;
  }

  if(pai->tse == epicsTimeEventDeviceTime) {
    /* do timestamp by device support */
    pai->time.secPastEpoch = __ld_le32(&(header->tv_sec));
    pai->time.nsec         = __ld_le32(&(header->tv_nsec));
  }
  
  if (bldImbReceiver->bsa_counter >=
      bldImbReceiver->payload_count + 2) {
    bldImbReceiver->bsa_counter = 0;
  }

  pai->udf = FALSE;
  
  return 2;
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
