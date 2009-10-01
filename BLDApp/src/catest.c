/*caExample.c*/
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cadef.h"

int catest(char * name)
{
    double	data;
    chid	mychid;

    SEVCHK(ca_context_create(ca_disable_preemptive_callback),"ca_context_create");
    SEVCHK(ca_create_channel(name,NULL,NULL,10,&mychid),"ca_create_channel failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
    SEVCHK(ca_get(DBR_DOUBLE,mychid,(void *)&data),"ca_get failure");
    SEVCHK(ca_pend_io(5.0),"ca_pend_io failure");
    printf("%s %f\n",name,data);
    SEVCHK(ca_clear_channel(mychid),"ca_create_channel failure");
    ca_context_destroy();
    return(0);
}
