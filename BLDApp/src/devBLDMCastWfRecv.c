/* $Id: devBLDMCastWfRecv.c,v 1.3 2010/04/27 00:44:39 strauman Exp $ */

/* Device support for a waveform record to receive
 * BLD multicast data and store all items in a waveform
 * 
 * Author: Till Straumann, 2010.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include <epicsExport.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsTime.h>
#include <epicsMessageQueue.h>
#include <errlog.h>

#include <dbLock.h>
#include <recSup.h>
#include <recGbl.h>
#include <devSup.h>
#include <alarm.h>

#include <dbFldTypes.h>

#include <waveformRecord.h>
#include <udpComm.h>

#include <BLDMCast.h>

#include <time.h>

#define QUEUE_DEPTH 20
#define N_INFO_VALS 7
#define __STR__(arg) #arg


typedef struct WfDpvtRec_ {
	epicsMessageQueueId que;
	UdpCommPkt          pkt;
} WfDpvtRec, *WfDpvt;

int bldmcwf_shuffled = 0;
epicsExportAddress(int, bldmcwf_shuffled);
int bldmcwf_tossed   = 0;
epicsExportAddress(int, bldmcwf_tossed);
int bldmcwf_tout     = 0;
epicsExportAddress(int, bldmcwf_tout);
int bldmcwf_good     = 0;
epicsExportAddress(int, bldmcwf_good);
unsigned bldmcwf_maxdiff  = 0;
epicsExportAddress(int, bldmcwf_maxdiff);
unsigned bldmcwf_mindiff  = -1;
epicsExportAddress(int, bldmcwf_mindiff);

double bldmcwf_max_block_s = 1.;
epicsExportAddress(double, bldmcwf_max_block_s);

/* This task just receives packets and sends them to the message queue.
 * We need this so that we can have the main worker block on a single
 * object (the queue) either for packets or for trigger events.
 */
static void
shuffler(void *arg)
{
int             sd, err;
waveformRecord  *p_wf = arg;
WfDpvt          p_dp = p_wf->dpvt;
UdpCommPkt      pkt;
char            *ifaddr;

	if ( (ifaddr = getenv("IPADDR1")) ) {
		udpCommSetIfMcastInp( inet_addr(ifaddr) );
	}

	if ( (sd = udpCommSocket( BLDMCAST_DST_PORT )) < 0 ) {
		errlogPrintf("Unable to create socket: %s\n", strerror(-sd));
		goto bail;
	}
	if ( (err = udpCommJoinMcast( sd, inet_addr( BLDMCAST_DST_IP ) )) ) {
		errlogPrintf("Unable to join MC group: %s\n", strerror(-err));
		goto bail;
	}

	do {
		/* There is no infinite timeout so just loop */
		while ( ! (pkt = udpCommRecv( sd, 100000 )) )
			;
		if ( epicsMessageQueueTrySend( p_dp->que, &pkt, sizeof(pkt) ) ) {
			errlogPrintf("shuffler: queue full?\n");
			udpCommFreePacket( pkt );
		} else {
			bldmcwf_shuffled++;
		}
	} while (1 );

bail:
	if ( sd >= 0 )
		udpCommClose( sd );

}


static void
worker(void *arg)
{
waveformRecord  *p_wf = arg;
WfDpvt          p_dp = p_wf->dpvt;
UdpCommPkt      pkt;
int             st;
struct timespec now, then;
unsigned        diff;

	while ( 0 <= (st = epicsMessageQueueReceive( p_dp->que, &pkt, sizeof(pkt) )) ) {
		if ( pkt ) {
			/* we're not currently waiting for a packet; just discard */
			udpCommFreePacket( pkt );
			bldmcwf_tossed++;
		} else {
			/* NULL pkt indicates a trigger event; we should now 
			 * wait for data.
			 */
			clock_gettime(CLOCK_REALTIME,&then);
			st = epicsMessageQueueReceiveWithTimeout( p_dp->que, &pkt, sizeof(pkt), bldmcwf_max_block_s );
			clock_gettime(CLOCK_REALTIME,&now);

			if ( then.tv_nsec > now.tv_nsec )
				now.tv_nsec += 1000000000;
			diff = now.tv_nsec - then.tv_nsec;
			diff /= 1000;

			if ( diff < bldmcwf_mindiff )
				bldmcwf_mindiff = diff;
			if ( diff > bldmcwf_maxdiff )
				bldmcwf_maxdiff = diff;

			if ( 0 > st) {
				/* Timed out; set 'pkt' to NULL to indicate that there was no data */
				pkt = 0;
				bldmcwf_tout++;
			} else {
				bldmcwf_good++;
			}
			p_dp->pkt = pkt;
			dbScanLock( (struct dbCommon*)p_wf );
				p_wf->rset->process( p_wf );
			dbScanUnlock( (struct dbCommon*)p_wf );
		}
	}

	errlogPrintf("worker: blocking for messages failed -- quitting\n");
}

static long
init_record(waveformRecord *p_wf)
{
WfDpvt      p_dp;
const char *msg;

	/* Make sure ftvl and nelm match our expectations */
	if ( p_wf->ftvl != DBF_DOUBLE ) {
		msg = "FTVL must be 'DOUBLE'";
		goto bail;
	}

	if ( p_wf->nelm != N_INFO_VALS ) {
		msg = "NELM must be "__STR__(N_INFO_VALS);
		goto bail;
	}


	if ( ! (p_wf->dpvt = p_dp = calloc( 1, sizeof( WfDpvtRec ) )) ) {
		msg = "No memory for DPVT struct";
		goto bail;
	}

	if ( ! (p_dp->que = epicsMessageQueueCreate( QUEUE_DEPTH, sizeof(UdpCommPkt) ) ) ) {
		msg = "Unable to create message queue";
		goto bail;
	}
	
	if ( ! epicsThreadCreate("BLDshuffler",
	                         epicsThreadPriorityBaseMax + 1,
	                         epicsThreadGetStackSize(epicsThreadStackMedium),
	                         shuffler,
	                         p_wf) ) {
		msg = "Unable to create 'shuffler' task";
		goto bail;
	}

	if ( ! epicsThreadCreate("BLDworker",
	                         epicsThreadPriorityBaseMax + 2,
	                         epicsThreadGetStackSize(epicsThreadStackMedium),
	                         worker,
	                         p_wf) ) {
		msg = "Unable to create 'worker' task";
		goto bail;
	}

	return 0;

bail:
	errlogPrintf("devBLDMCastWfRecv (init_record) -- ERROR: %s\n", msg);
	p_wf->pact = TRUE;
	return -1;
}

static double thisIsNan = 0./0.;

static long
read_wf(waveformRecord *p_wf)
{
UdpCommPkt pkt    = 0;
WfDpvt     p_dp   = p_wf->dpvt;
int        i;
double     *p_dbl = p_wf->bptr;

EBEAMINFO  *p_info;
uint32_t   dmg;

	if ( !p_wf->pact ) {
		if ( 0 == p_wf->rarm )
			return 0;

		/* start async processing; enqueue NULL packet telling the 'worker'
		 * that it should start listening for data.
		 */
		if ( 0 == epicsMessageQueueTrySend( p_dp->que, &pkt , sizeof(pkt) ) ) {
			/* success */
			p_wf->pact = TRUE;
		} else {
			/* unable to send 'sync' message */
			p_wf->udf  = TRUE;
			recGblSetSevr( p_wf, WRITE_ALARM, INVALID_ALARM );
			for ( i=0; i < N_INFO_VALS; i++ ) {
				p_dbl[i] = thisIsNan;
			}
		}
	} else {
		/* completion phase */
		if ( p_dp->pkt ) {
			p_info = udpCommBufPtr( p_dp->pkt );

			dmg      = __ld_le32( &p_info->uDamageMask  );

			p_dbl[0] = (dmg & (1<<0) ) ? thisIsNan : __ld_le64( &p_info->ebeamCharge   );
			p_dbl[1] = (dmg & (1<<1) ) ? thisIsNan : __ld_le64( &p_info->ebeamL3Energy );
			p_dbl[2] = (dmg & (1<<2) ) ? thisIsNan : __ld_le64( &p_info->ebeamLTUPosX  );
			p_dbl[3] = (dmg & (1<<3) ) ? thisIsNan : __ld_le64( &p_info->ebeamLTUPosY  );
			p_dbl[4] = (dmg & (1<<4) ) ? thisIsNan : __ld_le64( &p_info->ebeamLTUAngX  );
			p_dbl[5] = (dmg & (1<<5) ) ? thisIsNan : __ld_le64( &p_info->ebeamLTUAngY  );
			p_dbl[6] = (dmg & (1<<6) ) ? thisIsNan : __ld_le64( &p_info->ebeamBunchLen );
		} else {
			/* Nothing received */
			p_info = 0;
			p_wf->udf  = TRUE;
			recGblSetSevr( p_wf, READ_ALARM, INVALID_ALARM );
			for ( i=0; i < N_INFO_VALS; i++ ) {
				p_dbl[i] = thisIsNan;
			}
		}

		p_wf->nord = N_INFO_VALS;

		if ( epicsTimeEventDeviceTime == p_wf->tse ) {
			if ( p_info ) {
				p_wf->time.secPastEpoch = __ld_le32( &p_info->ts_sec  );
				p_wf->time.nsec         = __ld_le32( &p_info->ts_nsec );
			} else {
				epicsTimeGetCurrent( &p_wf->time );
			}
		}

		if ( p_dp->pkt ) {
			udpCommFreePacket( p_dp->pkt );
			p_dp->pkt = 0;
		}

		/* Disarm */
		if ( 1 == p_wf->rarm )
			p_wf->rarm = 0;
	}

	return 0;
}

static long
report(int interest)
{
	epicsPrintf("devBLDMCastWfRecv: receive BLD multicast packet and store data in a waveform\n");
	if ( interest ) {
		epicsPrintf("devBLDMCastWfRecv statistics:\n");
		epicsPrintf("   MC packets received and forwarded to worker        : %9u\n", bldmcwf_shuffled);
		epicsPrintf("   MC packets discarded by worker (record not scanned): %9u\n", bldmcwf_tossed);
		epicsPrintf("   No packets received in %7.4f"" seconds (timeout)   : %9u\n", bldmcwf_max_block_s, bldmcwf_tout);
		epicsPrintf("   Packets processed                                  : %9u\n", bldmcwf_good);
		epicsPrintf("   Min delay from fiducial to receive all FCOM blobs  : %9u uS\n", bldmcwf_mindiff);
		epicsPrintf("   Max delay from fiducial to receive all FCOM blobs  : %9u uS\n", bldmcwf_maxdiff);

		epicsPrintf("   Current FCOM timeout                               : %9.4f  S\n", bldmcwf_max_block_s);
	}

	return 0;
}

/* Define the DSET */

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;	
} devBLDMCastWfRecv = {
	5,
	report,
	NULL,
	init_record,
	NULL,
	read_wf
};

epicsExportAddress(dset, devBLDMCastWfRecv);
