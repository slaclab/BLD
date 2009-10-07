#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <epicsVersion.h>
#if EPICS_VERSION>=3 && EPICS_REVISION>=14

#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsInterrupt.h>
#include <cantProceed.h>
#include <epicsExport.h>
#include <devLib.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <callback.h>
#include <cvtTable.h>
#include <link.h>
#include <recSup.h>
#include <recGbl.h>
#include <devSup.h>
#include <drvSup.h>
#include <dbCommon.h>
#include <alarm.h>
#include <cantProceed.h>
#include <cadef.h>

#include <aiRecord.h>
#include <biRecord.h>
#include <longinRecord.h>

#else
#error "You need EPICS 3.14 or above because we need OSI support!"
#endif

#include "BLDMCast.h"

#define BLD_MCAST_DRV_VERSION "BLD_MCAST driver V1.0"

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
        BLD_MCAST_AI_CHARGE,
        BLD_MCAST_AI_ENERGY,
        BLD_MCAST_AI_POS_X,
        BLD_MCAST_AI_POS_Y,
        BLD_MCAST_AI_ANG_X,
        BLD_MCAST_AI_ANG_Y,
        BLD_MCAST_BI_PV_CON,
        BLD_MCAST_LI_INV_CNT,
        BLD_MCAST_LI_TS_CNT
} BLD_MCASTFUNC;

/*      define parameter check for convinence */
#define CHECK_AIPARM(PARM,VAL)\
        if (!strncmp(pai->inp.value.vmeio.parm,(PARM),strlen((PARM)))) {\
                pai->dpvt=(void *)VAL;\
                return (0);\
        }
#define CHECK_BIPARM(PARM,VAL)\
        if (!strncmp(pbi->inp.value.vmeio.parm,(PARM),strlen((PARM)))) {\
                pbi->dpvt=(void *)VAL;\
                return (0);\
        }
#define CHECK_LIPARM(PARM,VAL)\
        if (!strncmp(pli->inp.value.vmeio.parm,(PARM),strlen((PARM)))) {\
                pli->dpvt=(void *)VAL;\
                return (0);\
        }

static long init_ai( struct aiRecord * pai)
{
    pai->dpvt = NULL;

    if (pai->inp.type!=INST_IO)
    {
        recGblRecordError(S_db_badField, (void *)pai, "devAiBLD_MCAST Init_record, Illegal INP");
        pai->pact=TRUE;
        return (S_db_badField);
    }

    CHECK_AIPARM("CHARGE",      BLD_MCAST_AI_CHARGE)
    CHECK_AIPARM("ENERGY",      BLD_MCAST_AI_ENERGY)
    CHECK_AIPARM("POS_X",      BLD_MCAST_AI_POS_X)
    CHECK_AIPARM("POS_Y",      BLD_MCAST_AI_POS_Y)
    CHECK_AIPARM("ANG_X",      BLD_MCAST_AI_ANG_X)
    CHECK_AIPARM("ANG_Y",      BLD_MCAST_AI_ANG_Y)

    recGblRecordError(S_db_badField, (void *) pai, "devAiBLD_MCAST Init_record, bad parm");
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
    case BLD_MCAST_AI_CHARGE:
        pai->val = ebeamInfo.ebeamCharge;
        if(ebeamInfo.uDamageMask & 0x1)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_MCAST_AI_ENERGY:
        pai->val = ebeamInfo.ebeamL3Energy;
        if(ebeamInfo.uDamageMask & 0x2)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_MCAST_AI_POS_X:
        pai->val = ebeamInfo.ebeamLTUPosX;
        if(ebeamInfo.uDamageMask & 0x4)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_MCAST_AI_POS_Y:
        pai->val = ebeamInfo.ebeamLTUPosY;
        if(ebeamInfo.uDamageMask & 0x8)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_MCAST_AI_ANG_X:
        pai->val = ebeamInfo.ebeamLTUAngX;
        if(ebeamInfo.uDamageMask & 0x10)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    case BLD_MCAST_AI_ANG_Y:
        pai->val = ebeamInfo.ebeamLTUAngY;
        if(ebeamInfo.uDamageMask & 0x20)
            recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
        break;
    }

    if(pai->tse == epicsTimeEventDeviceTime)/* do timestamp by device support */
        pai->time = ebeamInfo.timestamp;

    if(mutexLock) epicsMutexUnlock(mutexLock);

    pai->udf=TRUE;
    return 2;
}

struct BLD_MCAST_DEV_SUP_SET
{
    long            number;
    DEVSUPFUN       report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record;
    DEVSUPFUN       get_ioint_info;
    DEVSUPFUN       read;
    DEVSUPFUN       special_linconv;
};

struct BLD_MCAST_DEV_SUP_SET devAiBLD_MCAST = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};
/*struct BLD_MCAST_DEV_SUP_SET devBiBLD_MCAST = {6, NULL, NULL, init_bi, NULL, read_bi, NULL};
struct BLD_MCAST_DEV_SUP_SET devLiBLD_MCAST = {6, NULL, NULL, init_li, NULL, read_li, NULL};*/

#if EPICS_VERSION>=3 && EPICS_REVISION>=14
epicsExportAddress(dset, devAiBLD_MCAST);
/*epicsExportAddress(dset, devBiBLD_MCAST);
epicsExportAddress(dset, devLiBLD_MCAST);*/
#endif

