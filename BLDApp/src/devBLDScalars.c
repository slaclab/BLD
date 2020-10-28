/*=============================================================================

  Name: devBLDScalars.c

  Abs:  BLD driver for scalar constants that are used in BLD calculations. 

  This follows a similar structure to the ai device support in debBLDMCastStatus

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
#include <aoRecord.h>
#include <errlog.h>

#include "BLDMCast.h"

/**
 * Analog Out (ao) record types
 */
static long init_ao ( aoRecord *precord );
static long write_ao( aoRecord *precord );

typedef struct
{
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   write;
    DEVSUPFUN   special_linconv;
} BLD_AO_DEV_SUP;

BLD_AO_DEV_SUP devAoBLD = {
    6,
    NULL,
    NULL,
    init_ao,
    NULL,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoBLD);

/**
 * BLDScalars
 * This enum is used to identify which ao record is being written to.
 * The enumeration value is stored in the record->dpvt field as a void *.
 * This value is used in the write_ao routine to determine which action should
 * be taken.
 */
typedef enum {
    BLD_SCALAR_ETAX,
} BLDScalars;

/**
 * Helper macro adapted from devBLDMCastStatus.c
 */
#define CHECK_AO_PARAM(PARAM,VAL)\
    if (!strncmp(precord->out.value.instio.string,(PARAM),strlen((PARAM)))) {\
        precord->dpvt=(void *)VAL;\
        return (0);\
    }

static long init_ao( aoRecord *precord )
{
    precord->dpvt = NULL;

    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)precord, "devAoBLD init_record, illegal INP");
        goto err; 
    }

    /**
     * Initialize the AO records
     * The first argument is the string in the record OUT field ("@Name") and
     * the second is the BLDScalars enumeration value it should match to.
     */
    CHECK_AO_PARAM("ETAX",    BLD_SCALAR_ETAX)

    /* Failure */
    recGblRecordError(S_db_badField, (void *) precord, "devAoBLD init_record, bad parm");
err:
    precord->pact = TRUE;
    return (S_db_badField);
}

static long write_ao( aoRecord *precord )
{
    if (bldMutex)
        epicsMutexLock(bldMutex);

    long scalarId = (long) precord->dpvt;
    switch (scalarId)
    {
    case BLD_SCALAR_ETAX:
        etaxPV = (double) precord->val;
        break;
    }

    if (bldMutex)
        epicsMutexUnlock(bldMutex);

    return 0;
}
