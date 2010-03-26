/* $Id: devBLDMCastStatus.c,v 1.8 2010/03/25 15:17:21 strauman Exp $ */

#include <string.h>
#include <stdlib.h>

#include <epicsMutex.h>
#include <epicsExport.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <alarm.h>
#include <aiRecord.h>

#include "BLDMCast.h"

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

    CHECK_AIPARM("CHARGE",     BLD_AI_CHARGE)
    CHECK_AIPARM("ENERGY",     BLD_AI_ENERGY)
    CHECK_AIPARM("POS_X",      BLD_AI_POS_X)
    CHECK_AIPARM("POS_Y",      BLD_AI_POS_Y)
    CHECK_AIPARM("ANG_X",      BLD_AI_ANG_X)
    CHECK_AIPARM("ANG_Y",      BLD_AI_ANG_Y)
    CHECK_AIPARM("BLEN",       BLD_AI_BLEN)

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
int damage = 0;

    if(mutexLock) epicsMutexLock(mutexLock);

    switch ((int)pai->dpvt)
    {
    case BLD_AI_CHARGE:
        pai->val = __ld_le64(&ebeamInfo.ebeamCharge);
        if(ebeamInfo.uDamageMask & __le32(0x1))
			damage = 1;
        break;
    case BLD_AI_ENERGY:
        pai->val = __ld_le64(&ebeamInfo.ebeamL3Energy);
        if(ebeamInfo.uDamageMask & __le32(0x2))
			damage = 1;
        break;
    case BLD_AI_POS_X:
        pai->val = __ld_le64(&ebeamInfo.ebeamLTUPosX);
        if(ebeamInfo.uDamageMask & __le32(0x4))
			damage = 1;
        break;
    case BLD_AI_POS_Y:
        pai->val = __ld_le64(&ebeamInfo.ebeamLTUPosY);
        if(ebeamInfo.uDamageMask & __le32(0x8))
			damage = 1;
        break;
    case BLD_AI_ANG_X:
        pai->val = __ld_le64(&ebeamInfo.ebeamLTUAngX);
        if(ebeamInfo.uDamageMask & __le32(0x10))
			damage = 1;
        break;
    case BLD_AI_ANG_Y:
        pai->val = __ld_le64(&ebeamInfo.ebeamLTUAngY);
        if(ebeamInfo.uDamageMask & __le32(0x20))
			damage = 1;
        break;
    case BLD_AI_BLEN:
        pai->val = __ld_le64(&ebeamInfo.ebeamBunchLen);
        if(ebeamInfo.uDamageMask & __le32(0x40))
			damage = 1;
        break;
    }

    if(pai->tse == epicsTimeEventDeviceTime) {
		/* do timestamp by device support */
        pai->time.secPastEpoch = __ld_le32(&ebeamInfo.ts_sec);
        pai->time.nsec         = __ld_le32(&ebeamInfo.ts_nsec);
	}

    if(mutexLock) epicsMutexUnlock(mutexLock);

    if ( damage )
        recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);

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

epicsExportAddress(dset, devAiBLD);
