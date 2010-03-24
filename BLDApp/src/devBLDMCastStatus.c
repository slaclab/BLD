/* $Id$ */

#include <string.h>
#include <stdlib.h>

#include <epicsVersion.h>

#if EPICS_VERSION>=3 && EPICS_REVISION>=14

#include <epicsMutex.h>
#include <epicsExport.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <alarm.h>
#include <aiRecord.h>

#else
#error "You need EPICS 3.14 or above because we need OSI support!"
#endif

#include "BLDMCast.h"

/* Share with device support */
extern EBEAMINFO ebeamInfo;
extern int bldAllPVsConnected;
extern int bldInvalidAlarmCount;
extern int bldUnmatchedTSCount;
extern epicsMutexId mutexLock;
extern IOSCANPVT  ioscan;         /* Trigger EPICS record */
/* Share with device support */

#define MAX_CA_STRING_SIZE (40)

/* define function flags */
typedef enum {
        BLD_AI_CHARGE,
        BLD_AI_ENERGY,
        BLD_AI_POS_X,
        BLD_AI_POS_Y,
        BLD_AI_ANG_X,
        BLD_AI_ANG_Y,
        BLD_AI_BLEN,
        BLD_BI_PV_CON,
        BLD_LI_INV_CNT,
        BLD_LI_TS_CNT
} BLDFUNC;

/*      define parameter check for convinence */
#define CHECK_AIPARM(PARM,VAL)\
        if (!strncmp(pai->inp.value.instio.string,(PARM),strlen((PARM)))) {\
                pai->dpvt=(void *)VAL;\
                return (0);\
        }

static long init_ai( struct aiRecord * pai)
{
    pai->dpvt = NULL;

    if (pai->inp.type!=INST_IO)
    {
        recGblRecordError(S_db_badField, (void *)pai, "devAiBLD Init_record, Illegal INP");
        pai->pact=TRUE;
        return (S_db_badField);
    }

    CHECK_AIPARM("CHARGE",      BLD_AI_CHARGE)
    CHECK_AIPARM("ENERGY",      BLD_AI_ENERGY)
    CHECK_AIPARM("POS_X",      BLD_AI_POS_X)
    CHECK_AIPARM("POS_Y",      BLD_AI_POS_Y)
    CHECK_AIPARM("ANG_X",      BLD_AI_ANG_X)
    CHECK_AIPARM("ANG_Y",      BLD_AI_ANG_Y)
    CHECK_AIPARM("BLEN",      BLD_AI_BLEN)

    recGblRecordError(S_db_badField, (void *) pai, "devAiBLD Init_record, bad parm");
    pai->pact = TRUE;

    return (S_db_badField);
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt)
{
    *iopvt = ioscan;
    return 0;
}

static long read_ai(struct aiRecord *pai)
{
    if(mutexLock) epicsMutexLock(mutexLock);
    switch ((int)pai->dpvt)
    {
    case BLD_AI_CHARGE:
        pai->val = ebeamInfo.ebeamCharge;
        if(ebeamInfo.uDamageMask & 0x1)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_ENERGY:
        pai->val = ebeamInfo.ebeamL3Energy;
        if(ebeamInfo.uDamageMask & 0x2)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_POS_X:
        pai->val = ebeamInfo.ebeamLTUPosX;
        if(ebeamInfo.uDamageMask & 0x4)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_POS_Y:
        pai->val = ebeamInfo.ebeamLTUPosY;
        if(ebeamInfo.uDamageMask & 0x8)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_ANG_X:
        pai->val = ebeamInfo.ebeamLTUAngX;
        if(ebeamInfo.uDamageMask & 0x10)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_ANG_Y:
        pai->val = ebeamInfo.ebeamLTUAngY;
        if(ebeamInfo.uDamageMask & 0x20)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_AI_BLEN:
        pai->val = ebeamInfo.ebeamBunchLen;
        if(ebeamInfo.uDamageMask & 0x40)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    }

    if(pai->tse == epicsTimeEventDeviceTime)/* do timestamp by device support */
        pai->time = ebeamInfo.timestamp;

    if(mutexLock) epicsMutexUnlock(mutexLock);

    pai->udf=FALSE;
    return 2;
}

struct BLD_DEV_SUP_SET
{
    long            number;
    DEVSUPFUN       report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record;
    DEVSUPFUN       get_ioint_info;
    DEVSUPFUN       read;
    DEVSUPFUN       special_linconv;
};

struct BLD_DEV_SUP_SET devAiBLD = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devAiBLD);
#endif

