/*caMonitor.c*/
/* This example accepts a file containing a list of pvs to monitor
 * It prints a message for all ca evemts: connection, access rights, data
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"
#include "dbDefs.h"
#include "db_access.h"

#define MAX_PV 1000
#define MAX_PV_NAME_LEN 40

typedef struct{
    char		pvname[60];
    chid		mychid;
    evid		myevid;
} MYNODE;


static void printChidInfo(chid chid, char *message)
{
    printf("\n%s\n",message);
    printf("pv: %s  type(%d) nelements(%ld) host(%s)",
	ca_name(chid),ca_field_type(chid),ca_element_count(chid),
	ca_host_name(chid));
    printf(" read(%d) write(%d) state(%d)\n",
	ca_read_access(chid),ca_write_access(chid),ca_state(chid));
}

static void exceptionCallback(struct exception_handler_args args)
{
    chid	chid = args.chid;
    long	stat = args.stat; /* Channel access status code*/
    const char  *channel;
    static char *noname = "unknown";

    channel = (chid ? ca_name(chid) : noname);


    if(chid) printChidInfo(chid,"exceptionCallback");
    printf("exceptionCallback stat %s channel %s\n",
        ca_message(stat),channel);
}

static void eventCallback(struct event_handler_args eha)
{
    chid	chid = eha.chid;

    if(eha.status!=ECA_NORMAL) {
	printChidInfo(chid,"eventCallback");
    } else {
	struct dbr_time_double	*pdata = (struct dbr_time_double *)eha.dbr;
        errlogPrintf("Event Callback: %s, copy type %ld, elements %ld, bytes %d, severity %d\n",
                ca_name(eha.chid), eha.type, eha.count, dbr_size_n(eha.type, eha.count), pdata->severity);
            errlogPrintf("Value is %f\n", pdata->value);

    }
}

int camon(const char * pvname, unsigned long count)
{
    MYNODE	*pmynode;

    pmynode = (MYNODE *)calloc(1,sizeof(MYNODE));
    strcpy(pmynode->pvname, pvname);

    SEVCHK(ca_context_create(ca_enable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_add_exception_event(exceptionCallback,NULL), "ca_add_exception_event");

    SEVCHK(ca_create_channel(pmynode->pvname,NULL, NULL,20,&(pmynode->mychid)), "ca_create_channel");
    SEVCHK(ca_pend_io(10.0),"ca_pend_event");
#if 1
    if(count>1)
    {
        SEVCHK(ca_add_array_event(DBR_TIME_DOUBLE, count, pmynode->mychid,eventCallback, pmynode,0.0, 0.0, 0.0,&(pmynode->myevid)), "ca_add_array_event");
    }
    else
    {
        SEVCHK(ca_add_event(DBR_TIME_DOUBLE,pmynode->mychid,eventCallback, pmynode,&(pmynode->myevid)), "ca_add_event");
    }
#else
    SEVCHK(ca_create_subscription(DBR_TIME_DOUBLE, 0, pmynode->mychid, DBE_VALUE|DBE_ALARM, eventCallback, pmynode, &(pmynode->myevid)), "ca_create_subscription");
#endif
    ca_flush_io();

    while('q' != getchar()){};
#if 1
    ca_clear_event(pmynode->myevid);
#else
    ca_clear_subscription(pmynode->myevid);
#endif
    ca_clear_channel(pmynode->mychid);
    return(0);
}

