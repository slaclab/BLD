/* BLDMCast.c */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/uio.h>
#include <net/if.h>

#include "epicsVersion.h"
#include "epicsExport.h"
#include "cadef.h"
#include "dbDefs.h"
#include "db_access.h"
#include "alarm.h"
#include "cantProceed.h"
#include "errlog.h"

#include "BLDMCast.h"

int BLD_MCAST_ENABLE = 1;
epicsExportAddress(int, BLD_MCAST_ENABLE);

int BLD_MCAST_DEBUG = 0;
epicsExportAddress(int, BLD_MCAST_DEBUG);

static epicsEventId EVRFireEvent = NULL;
static epicsMutexId mutexLock = NULL;	/* Protect staticPVs' values */

static int BLDMCastTask(void * parg);

int BLDMCastStart(int enable)
{/* This funciton will be called in st.cmd after iocInit() */
    /* Do we need to use RTEMS task priority to get higher priority? */
    BLD_MCAST_ENABLE = enable;
    return (epicsThreadMustCreate("BLDMCast", TASK_PRIORITY, 20480, (EPICSTHREADFUNC)BLDMCastTask, NULL));
}

int EVRFire()
{/* This funciton will be registered with EVR callback */
    if(EVRFireEvent) epicsEventSignal(EVRFireEvent);
    return 0;
}

static void printChIdInfo(chid pvChId, char *message)
{
    errlogPrintf("\n%s\n",message);
    errlogPrintf("pv: %s  type(%d) nelements(%ld) host(%s)",
	ca_name(pvChId),ca_field_type(pvChId),ca_element_count(pvChId),ca_host_name(pvChId));
    errlogPrintf(" read(%d) write(%d) state(%d)\n", ca_read_access(pvChId),ca_write_access(pvChId),ca_state(pvChId));
}

static void exceptionCallback(struct exception_handler_args args)
{
    chid	pvChId = args.chid;
    long	stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (pvChId ? ca_name(pcChId) : noname);

    if(pvChId) printChIdInfo(pvChId,"exceptionCallback");
    errlogPrintf("exceptionCallback stat %s channel %s\n", ca_message(stat),channel);
}

static void accessRightsCallback(struct access_rights_handler_args args)
{
    chid	pvChId = args.chid;

    if(BLD_MCAST_DEBUG >= 2) printChIdInfo(pvChId,"accessRightsCallback");
}

static void connectionCallback(struct connection_handler_args args)
{
    chid	pvChId = args.chid;

    if(BLD_MCAST_DEBUG >= 2) printChIdInfo(pvChId,"connectionCallback");
}

static void eventCallback(struct event_handler_args args)
{
    BLDPV * pPV = args.usr;

    if(BLD_MCAST_DEBUG >= 2) errlogPrintf("Event Callback: %s, status %d\n", ca_name(args.chid), args.status);

    epicsMutexLock(mutexLock);
    if (args.status == ECA_NORMAL)
    {
        memcpy(pPV->pTD, args.dbr, dbr_size_n(args.type, args.count));
        if(BLD_MCAST_DEBUG) errlogPrintf("Event Callback: %s, copy type %d, elements %d\n", ca_name(args.chid), args.type, args.count);
        if(pPV->pTD->severity >= INVALID_ALARM)	/* We don't care timestamp for staticPVs. As long as it is not invalid, they are ok */
            pPV->dataAvailable = FALSE;
        else
            pPV->dataAvailable = TRUE;
    }
    else
    {
        pPV->dataAvailable = FALSE;
    }
    epicsMutexUnlock(mutexLock);
}

static int BLDMCastTask(void * parg)
{
    int		loop;
    int		rtcode;
    BOOL	dataValid = FALSE;

    int		sFd;
    sockaddr_in sockaddrSrc;

    mutexLock = epicsMutexMustCreate();

    /************************************************************************* Prepare CA *************************************************************************/
    SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL), "ca_add_exception_event");

    /* Monitor static PVs */
    for(loop=0; loop<N_STATIC_PVS; loop++)
    {
        rtncode = ECA_NORMAL;
	SEVCHK(ca_create_channel(staticPVs[loop].name, connectionCallback, &(staticPVs[loop]), DEFAULT_CA_PRIORITY ,&(staticPVs[loop].pvChId)), "ca_create_channel");
	SEVCHK(ca_replace_access_rights_event(staticPVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event");

        /* We could do subscription in connetion callback. But in this case, better to enforce all connection */
        rtncode = ca_pend_io(2.0);
        if (rtncode == ECA_TIMEOUT)
        {
            errlogPrintf("Channel connect timed out: '%s' not found.\n", staticPVs[loop].name);
            return -1;
        }
        if(staticPVs[loop].nElems != ca_element_count(staticPVs[loop].pvChId))
        {
            errlogPrintf("Number of elements of '%s' does not match expectation.\n", staticPVs[loop].name);
            return -1;
        }

        if(DBF_DOUBLE != ca_field_type(staticPVs[loop].pvChId))
        {/* Native data type has to be double */
            errlogPrintf("Native data type of '%s' is not double.\n", staticPVs[loop].name);
            return -1;
        }

        /* Everything should be double, even not, do conversion */
        staticPVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, staticPVs[loop].nElems), "callocMustSucceed");
        SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, 0, staticPVs[loop].pvChId, DBE_VALUE|DBE_ALARM, eventCallback, &(staticPVs[loop]), NULL), "ca_create_subscription");
    }

    /* ca_flush_io(); */
    /* ca_pend_event(2.0); */

    /* We only fetch pulse PVs */
    for(loop=0; loop<N_PULSE_PVS; loop++)
    {
        rtncode = ECA_NORMAL;
	SEVCHK(ca_create_channel(pulsePVs[loop].name, connectionCallback, &(pulsePVs[loop]), DEFAULT_CA_PRIORITY ,&(pulsePVs[loop].pvChId)), "ca_create_channel");
	SEVCHK(ca_replace_access_rights_event(pulsePVs[loop].pvChId, accessRightsCallback), "ca_replace_access_rights_event");

        /* We could do subscription in connetion callback. But in this case, better to enforce all connection */
        rtncode = ca_pend_io(2.0);
        if (rtncode == ECA_TIMEOUT)
        {
            errlogPrintf("Channel connect timed out: '%s' not found.\n", pulsePVs[loop].name);
            return -1;
        }
        if(pulsePVs[loop].nElems != ca_element_count(pulsePVs[loop].pvChId))
        {
            errlogPrintf("Number of elements of '%s' does not match expectation.\n", pulsePVs[loop].name);
            return -1;
        }

        if(DBF_DOUBLE != ca_field_type(pulsePVs[loop].pvChId))
        {/* native type has to be double */
            errlogPrintf("Native data type of '%s' is not double.\n", pulsePVs[loop].name);
            return -1;
        }

        /* Everything should be double, even not, do conversion */
        pulsePVs[loop].pTD = callocMustSucceed(1, dbr_size_n(DBR_TIME_DOUBLE, pulsePVs[loop].nElems), "callocMustSucceed");
        /* We don't subscribe to pulse PVs */
    }

    /************************************************************************* Prepare MultiCast *******************************************************************/







    /* All ready to go, create event and register with EVR */
    EVRFireEvent = epicsEventMustCreate(epicsEventEmpty);
    /* TODO, register EVRFire */

    while(1)
    {
        int status;
        status = epicsEventWaitWithTimeout(EVRFireEvent, DEFAULT_EVR_TIMEOUT);
        if(status != epicsEventWaitOK)
        {
            if(status == epicsEventWaitTimeout)
            {
                errlogPrintf("Wait EVR timeout, check timing?\n");
                continue;
            }
            else
            {
                errlogPrintf("Wait EVR Error, what happened? Let's sleep 2 seconds.\n");
                epicsThreadSleep(2.0);
                continue;
            }
        }

        /* EVR fires ok, get pulse PVs */
        rtncode = ECA_NORMAL;
        for(loop=0; loop<N_PULSE_PVS; loop++)
        {
            if(pulsePVs[loop].nElems > 1)
            {
                rtncode = ca_array_get(DBR_TIME_DOUBLE, pulsePVs[loop].nElems, pulsePVs[loop].pvChId, (void *)(pulsePVs[loop].pTD));
            }
            else
            {
                rtncode = ca_get(DBR_TIME_DOUBLE, pulsePVs[loop].pvChId, (void *)(pulsePVs[loop].pTD));
            }
        }
        rtncode = ca_pend_io(0.03);
        if (rtncode != ECA_NORMAL)
        {
            if(BLD_MCAST_DEBUG) errlogPrintf("Something wrong when fetch pulse-by-pulse PVs.\n");
            continue;
        }

       /* Got all PVs, do calculation including checking dataAvailability and timestamp */
       epicsMutexLock(mutexLock);
       /* Data validation */
       dataValid = TRUE;
       ebeamInfo.timestamp = pulsePVs[0].pTD->stamp;

       for(loop=0; loop<N_STATIC_PVS; loop++)
       {
           if(!staticPVs[loop].dataAvailable)
           {
               dataValid = FALSE;
               break;
           }
       }

       for(loop=0; loop<N_PULSE_PVS; loop++)
       {
           if(pulsePVs[loop].pTD->severity >= INVALID_ALARM)
           {
               pulsePVs[loop].dataAvailable = FALSE;
               dataValid = FALSE;
               break;
           }
           if(memcmp
       }

       /* Calculate Beam Charge */
       epicsMutexLock(mutexUnlock);

       /* do MultiCast */
       if(BLD_MCAST_ENABLE)
       {
       }

    }

    /*Should never return from following call*/
    /*SEVCHK(ca_pend_event(0.0),"ca_pend_event");*/
    return(0);
}

