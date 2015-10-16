/* $Id: devBLDMCastStatus.c,v 1.13.2.2 2015/10/16 09:16:31 bhill Exp $ */


/*=============================================================================

  Name: devBLDMCastWfRecv.c

  Abs:  BLDMCast driver for eBeam MCAST BLD data sent to PCD.

  Auth: Sheng Peng (pengs)
  Mod:	Till Straumann (strauman)
  		Luciano Piccoli (lpiccoli)
  		Shantha Condamoor (scondam)
  		Bruce Hill (bhill)

  Mod:  22-Sep-2009 - S.Peng - Initial Release
		18-May-2010 - T.Straumann: BLD-R2-0-0-BR - Cleanup and modifications
		12-May-2011 - L.Piccoli	- Modifications
		11-Jun-2013 - L.Piccoli	- BLD-R2-2-0 - 	Addition of BLD receiver - phase cavity  
		30-Sep-2013 - L.Piccoli - BLD-R2-3-0 - Addition of Fast Undulator Launch feedback states, version 0x4000f
		27-Feb-2014 - L.Piccoli - Merged R2-0-0-BR with MAIN_TRUNK; HEAD, BLD-R2-5-5, BLD-R2-5-4, BLD-R2-5-3, BLD-R2-5-2, BLD-R2-5-1,
								 BLD-R2-5-0, BLD-R2-4-6, BLD-R2-4-5, BLD-R2-4-4, BLD-R2-4-3, BLD-R2-4-2, BLD-R2-4-1, BLD-R2-4-0 	   
		7-Jul-2014  - S.Condamoor - BLD-R2-6-0 - Added PHOTONENERGY, LTU450_POS_X,  LTU250_POS_X. Version 0x6000f
												Added code to set the 0x20000 damage bit if the EPICS variables become disconnected, or
												    if the BPM data is unavailable.
		18-Sep-2015 - B.Hill - Modified for linux compatibility
-----------------------------------------------------------------------------*/

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
#include <errlog.h>

#include "BLDMCast.h"

extern int BLD_MCAST_DEBUG;

/* define function flags */
typedef enum
{
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
  BLD_AI_PHOTONENERGY,  
  BLD_AI_LTU450_POS_X,   
  BLD_AI_LTU250_POS_X,  
  BLD_AI_DMP_CHARGE,  
  BLD_AI_XTCAV_AMP,
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
    CHECK_AIPARM("BC2CHARGE",  BLD_AI_BC2CHARGE)
    CHECK_AIPARM("BC2ENERGY",  BLD_AI_BC2ENERGY)
    CHECK_AIPARM("BC1CHARGE",  BLD_AI_BC1CHARGE)
    CHECK_AIPARM("BC1ENERGY",  BLD_AI_BC1ENERGY)

	CHECK_AIPARM("UND_POS_X",  BLD_AI_UND_POS_X);
	CHECK_AIPARM("UND_POS_Y",  BLD_AI_UND_POS_Y);
	CHECK_AIPARM("UND_ANG_X",  BLD_AI_UND_ANG_X);
	CHECK_AIPARM("UND_ANG_Y",  BLD_AI_UND_ANG_Y);
	
	CHECK_AIPARM("DMP_CHARGE", BLD_AI_DMP_CHARGE);
	CHECK_AIPARM("XTCAV_AMP",  BLD_AI_XTCAV_AMP);

	CHECK_AIPARM("PHOTONENERGY",BLD_AI_PHOTONENERGY);
	CHECK_AIPARM("LTU450_POS_X",BLD_AI_LTU450_POS_X);  
	CHECK_AIPARM("LTU250_POS_X",BLD_AI_LTU250_POS_X);   

	recGblRecordError(S_db_badField, (void *) pai, "devAiBLD Init_record, bad parm");
	pai->pact = TRUE;

	return (S_db_badField);
}


/** for sync scan records  **/
static long ai_ioint_info(int cmd,aiRecord *pai,IOSCANPVT *iopvt)
{
  *iopvt = bldIoscan;
  return 0;
}

static long read_ai(struct aiRecord *pai)
{
  int damage = 0;

  if(bldMutex) epicsMutexLock(bldMutex);

  long int	tyData = (long int) pai->dpvt;
  switch ( tyData ) {
  case BLD_AI_CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamCharge);
    if(bldEbeamInfo.uDamageMask & __le32(EBEAMCHARGE_DAMAGEMASK)) {
      damage = 1;
    }
    if (BLD_MCAST_DEBUG == 4) {
      unsigned int	fid = bldEbeamInfo.ts_nsec & 0x0001FFFF;
      errlogPrintf( "devBLDMCastStatus.c:read_ai charge=%1.3e fid=%u\n", pai->val, fid );
    }
    break;
  case BLD_AI_ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamL3Energy);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAML3ENERGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUPosX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTUPOSX_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_POS_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUPosY);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTUPOSY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_ANG_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUAngX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTUANGX_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_ANG_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTUAngY);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTUANGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_BLEN:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Current);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMBC2CURRENT_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC2CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Current);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMBC2CURRENT_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC2ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC2Energy);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMBC2ENERGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC1CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC1Current);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMBC1CURRENT_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_BC1ENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamBC1Energy);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMBC1ENERGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndPosX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMUNDPOSX_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_ANG_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndAngX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMUNDANGX_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_POS_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndPosY);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMUNDPOSY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_UND_ANG_Y:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamUndAngY);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMUNDANGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_DMP_CHARGE:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamDMP502Charge);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMDMP502CHARGE_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_XTCAV_AMP:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamXTCAVAmpl);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMXTCAVAMPL_DAMAGEMASK)) {
      damage = 1;
    }
    break;
  case BLD_AI_PHOTONENERGY:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamPhotonEnergy);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMPHTONENERGY_DAMAGEMASK)) {
      damage = 1;
    }
    break;	
  case BLD_AI_LTU450_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTU450PosX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTU450POSX_DAMAGEMASK)) {
      damage = 1;
    }
    break;	
  case BLD_AI_LTU250_POS_X:
    pai->val = __ld_le64(&bldEbeamInfo.ebeamLTU250PosX);
	if(bldEbeamInfo.uDamageMask & __le32(EBEAMLTU250POSX_DAMAGEMASK)) {
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

#include <subRecord.h>
#include <registryFunction.h>
#include <epicsExport.h>

long subInit(struct subRecord *psub)
{
  errlogPrintf("subInit was called\n");
  return 0;
}

long subProcess(struct subRecord *psub)
{
  psub->val = (int)(psub->time.nsec & 0x1FFFF);
  return 0;
}

epicsRegisterFunction(subInit);
epicsRegisterFunction(subProcess);

epicsExportAddress(dset, devAiBLD);
