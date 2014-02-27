/* $Id: devBLDMCastStatus.c,v 1.10.2.5 2013/09/30 18:31:35 lpiccoli Exp $ */

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

extern int BLD_MCAST_DEBUG;

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
  BLD_LI_TS_CNT,
  BLD_AI_BC2CHARGE,
  BLD_AI_BC2ENERGY,
  BLD_AI_BC1CHARGE,
  BLD_AI_BC1ENERGY,
  BLD_AI_UND_POS_X,
  BLD_AI_UND_POS_Y,
  BLD_AI_UND_ANG_X,
  BLD_AI_UND_ANG_Y,	
} BLDFUNC;

/*      define parameter check for convinence */
#define CHECK_AIPARM(PARM,VAL)\
        if (!strncmp(pai->inp.value.instio.string,(PARM),strlen((PARM)))) {\
                pai->dpvt=(void *)VAL;\
                return (0);\
        }

static long init_ai( struct aiRecord * pai) {
  pai->dpvt = NULL;
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
    CHECK_AIPARM("BC2CHARGE",  BLD_AI_BC2CHARGE)
    CHECK_AIPARM("BC2ENERGY",  BLD_AI_BC2ENERGY)
    CHECK_AIPARM("BC1CHARGE",  BLD_AI_BC1CHARGE)
    CHECK_AIPARM("BC1ENERGY",  BLD_AI_BC1ENERGY)

  if (pai->inp.type!=INST_IO) {
    recGblRecordError(S_db_badField, (void *)pai, "devAiBLD Init_record, Illegal INP");
    pai->pact=TRUE;
    return (S_db_badField);
  }

  CHECK_AIPARM("CHARGE",     BLD_AI_CHARGE);
  CHECK_AIPARM("ENERGY",     BLD_AI_ENERGY);
  CHECK_AIPARM("POS_X",      BLD_AI_POS_X);
  CHECK_AIPARM("POS_Y",      BLD_AI_POS_Y);
  CHECK_AIPARM("ANG_X",      BLD_AI_ANG_X);
  CHECK_AIPARM("ANG_Y",      BLD_AI_ANG_Y);
  CHECK_AIPARM("BC2CHARGE",  BLD_AI_BC2CHARGE);
  CHECK_AIPARM("BC2ENERGY",  BLD_AI_BC2ENERGY);
  CHECK_AIPARM("BC1CHARGE",  BLD_AI_BC2CHARGE);
  CHECK_AIPARM("BC1ENERGY",  BLD_AI_BC2ENERGY);
  CHECK_AIPARM("UND_POS_X",  BLD_AI_UND_POS_X);
  CHECK_AIPARM("UND_POS_Y",  BLD_AI_UND_POS_Y);
  CHECK_AIPARM("UND_ANG_X",  BLD_AI_UND_ANG_X);
  CHECK_AIPARM("UND_ANG_Y",  BLD_AI_UND_ANG_Y);

  recGblRecordError(S_db_badField, (void *) pai, "devAiBLD Init_record, bad parm");
  pai->pact = TRUE;

  return (S_db_badField);
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt) {
  *iopvt = bldIoscan;
  return 0;
}

static long read_ai(struct aiRecord *pai) {
  int damage = 0;

  if(bldMutex) epicsMutexLock(bldMutex);

  switch ((int)pai->dpvt) {
  case BLD_AI_CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamCharge);
    if(bldEbeamInfo.uDamageMask & __le32(0x1)) {
      damage = 1;
    }
    if (BLD_MCAST_DEBUG == 1) {
      epicsUInt32 idcmp;
      idcmp = bldEbeamInfo.ts_nsec & 0x0001FFFF;
      printf ("%d\n", idcmp);
    }
    break;
  case BLD_AI_ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamL3Energy);
    if(bldEbeamInfo.uDamageMask & __le32(0x2)) {
      damage = 1;
    }
    break;
  case BLD_AI_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUPosX);
    if(bldEbeamInfo.uDamageMask & __le32(0x4)) {
      damage = 1;
    }
    break;
  case BLD_AI_POS_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUPosY);
    if(bldEbeamInfo.uDamageMask & __le32(0x8)) {
      damage = 1;
    }
    break;
  case BLD_AI_ANG_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUAngX);
    if(bldEbeamInfo.uDamageMask & __le32(0x10)) {
      damage = 1;
    }
    break;
  case BLD_AI_ANG_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUAngY);
    if(bldEbeamInfo.uDamageMask & __le32(0x20)) {
      damage = 1;
    }
    break;
  case BLD_AI_BLEN:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Current);
    if(bldEbeamInfo.uDamageMask & __le32(0x40)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC2CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Current);
    if(bldEbeamInfo.uDamageMask & __le32(0x40)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC2ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Energy);
    if(bldEbeamInfo.uDamageMask & __le32(0x80)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC1CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC1Current);
    if(bldEbeamInfo.uDamageMask & __le32(0x100)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC1ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC1Energy);
    if(bldEbeamInfo.uDamageMask & __le32(0x200)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndPosX);
    if(bldEbeamInfo.uDamageMask & __le32(0x400)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_ANG_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndAngX);
    if(bldEbeamInfo.uDamageMask & __le32(0x800)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_POS_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndPosY);
    if(bldEbeamInfo.uDamageMask & __le32(0x1000)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_ANG_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndAngY);
    if(bldEbeamInfo.uDamageMask & __le32(0x2000)) {
      damage = 1;
    }
    break;

  }

  if(pai->tse == epicsTimeEventDeviceTime) {
    /* do timestamp by device support */
    pai->time.secPastEpoch = __ld_le32(&bldEbeamInfo.ts_sec);
    pai->time.nsec         = __ld_le32(&bldEbeamInfo.ts_nsec);
  }

  if(bldMutex) {
    epicsMutexUnlock(bldMutex);
  }

  if (damage) {
    recGblSetSevr(pai, CALC_ALARM, INVALID_ALARM);
  }

  pai->udf=FALSE;
  
  return 2;
}

struct BLD_DEV_SUP_SET {
  long            number;
  DEVSUPFUN       report;
  DEVSUPFUN       init;
  DEVSUPFUN       init_record;
  DEVSUPFUN       get_ioint_info;
  DEVSUPFUN       read;
  DEVSUPFUN       special_linconv;
};

struct BLD_DEV_SUP_SET devAiBLD = {6, NULL, NULL, init_ai, ai_ioint_info, read_ai, NULL};

#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

long subInit(struct subRecord *psub) {
  printf("subInit was called\n");
  return 0;
}

long subProcess(struct subRecord *psub) {
  psub->val = (int)(psub->time.nsec & 0x1FFFF);
  return 0;
}

epicsRegisterFunction(subInit);
epicsRegisterFunction(subProcess);

epicsExportAddress(dset, devAiBLD);
