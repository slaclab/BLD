/*=============================================================================

  Name: devBLDMCastConstants.c

  Abs:  BLDMCast driver for constants that are used in BLD calculations. 

  Auth: Ryan Reno (rreno)

  Mod:  08-Oct-2020 - R.Reno - Initial Release
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
#include <longoutRecord.h>
#include <errlog.h>

#include "BLDMCast.h"

static long init_longout ( longoutRecord *precord );
static long write_longout( longoutRecord *precord );

struct BLD_DEV_SUP_SET
{
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   write;
    DEVSUPFUN   special_linconv;
};

struct BLD_DEV_SUP_SET devLongoutBLD = { 6, NULL, NULL, init_longout, NULL, write_longout, NULL };
epicsExportAddress(dset, devLongoutBLD);

typedef enum {
    BLD_SCALAR_ETAX,
} BLDScalars;

#define CHECK_PARM(PARM,VAL)\
    if (!strncmp(precord->out.value.instio.string,(PARM),strlen((PARM)))) {\
        precord->dpvt=(void *)VAL;\
        return (0);\
    }

static long init_longout( longoutRecord *precord )
{
    precord->dpvt = NULL;

    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)precord, "devLongoutBLD init_record, illegal INP");
        goto err; 
    }

    /* Initialize the scalar records */
    CHECK_PARM("ETAX",    BLD_SCALAR_ETAX)

    /* Failure */
    recGblRecordError(S_db_badField, (void *) precord, "devLongoutBLD init_record, bad parm");
err:
    precord->pact = TRUE;
    return (S_db_badField);
}

static long write_longout( longoutRecord *precord )
{
    if (bldMutex)
        epicsMutexLock(bldMutex);

    long type = (long) precord->dpvt;
    switch (type)
    {
    case BLD_SCALAR_ETAX:
        etaxPV = (long) precord->val;
        break;
    }

    if (bldMutex)
        epicsMutexUnlock(bldMutex);

    return 0;
}
