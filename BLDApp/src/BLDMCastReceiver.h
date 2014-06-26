/* $Id: BLDMCastReceiver.h,v 1.7 2014/06/25 00:59:15 scondam Exp $ */

#ifndef _BLDMCASTRECEIVER_H_
#define _BLDMCASTRECEIVER_H_

#include "BLDData.h"

/**
 * BLDMCastReceiver.h
 *
 * Definition of functions and data fields to handle incoming BLD data packets.
 *
 * For each type of BLD packets there is one instance of this struct.
 *
 * Two tasks are created to receive and process BLD packets. The first task
 * (function bld_receiver_run()) waits for multicast data and saves it
 * to a tmp_buf to update the BSA PVs.
 */
struct BLDMCastReceiver {
  /** Socket for receiving multicast BLD packets */
  int sock;

  /** Multicast address (string) */
  char *multicast_group;

  /** Port number */
  int port;

  /** Latest BLD header - passed to recvmsg */
  BLDHeader *bld_header_recv; 

  /** Latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_recv; 

  /**
   * Copy of the latest BLD header, used by the BSA device support
   * to fill in the PVs. This memory is shared between the device
   * support and the receiver.
   */
  BLDHeader *bld_header_bsa; 

  /* Copy of the latest BLD payload - contiguous memory after bld_header */
  void *bld_payload_bsa; 

  /** Size of BLD data following the header */
  int payload_size; 

  /** Number of PVs to be updated (Number of items in BLD payload */
  int payload_count;

  /** Mutex for controlling access to this struct */
  epicsMutexId mutex;
  
  /** Number of BLD packets received */
  long packets_received;

  /** Number of BLD packets that are duplicates (same pulseID in header) */
  long packets_duplicates;  
  
  /** Count late BLDs based on differences between EVG timestamp and BLD header timestamp for same pulseid */
  long late_bld_pulse;  
  
    /** Count missed BLDs. There was no match within the last 2800 fiducials for a missed pulseid. May be it came too late? */
  long miss_bld_pulse;
  
    /** Last BLD pulseid received */
  Uint32_LE last_bld_pulse;    

	/* last BLD packet received time */
  epicsTimeStamp	bld_recv_time;  
  
	/* previous (one before last) BLD packet received time */	   
  epicsTimeStamp	previous_bld_time;
  
	/* difference between  previous_bld_time and bld_recv_time */	  
  double			bld_diffus;
  double			bld_diffus_max;
  double			bld_diffus_min;
  double			bld_diffus_avg; 
  
  /** Print status information (dbior command) */
  void (*report)(void *bld_receiver, int level); 
  
  /** Run the BLD receiver */
  void (*run)(void *bld_receiver);
};

typedef struct BLDMCastReceiver BLDMCastReceiver;

int bld_receiver_create(BLDMCastReceiver **this, int payload_size, int payload_count,
			char *multicast_group, int port);
int bld_receiver_destroy(BLDMCastReceiver *this);
int bld_receiver_next(BLDMCastReceiver *this);
void bld_receiver_report(void *this, int level);
void bld_receiver_run(BLDMCastReceiver *this);

void bld_receivers_start();
void bld_receivers_report(int level);

#define BLD_PhaseCavity_GROUP "239.255.24.1"
/*XRT */
#define BLD_HxxUm6Imb01_GROUP "239.255.24.4"
#define BLD_HxxUm6Imb02_GROUP "239.255.24.5"

typedef enum {
	EBeam,					/* 239.255.24.0 Global */
	PhaseCavity,			/* 239.255.24.1 Global */
	FEEGasDetEnergy,		/* 239.255.24.2 Global */
	Nh2Sb1Ipm01,			/* 239.255.24.3 XPP (in SXR) */
	HxxUm6Imb01,			/* 239.255.24.4 (XCS-IPM-01) XRT (controls) */
	HxxUm6Imb02,			/* 239.255.24.5 (XCS-DIO-01) XRT (controls) */
	HfxDg2Imb01,			/* 239.255.24.6 (XCS-IPM-02) XRT (controls) */
	HfxDg2Imb02,			/* 239.255.24.7 (XCS-DIO-02) XRT (controls) */
	XcsDg3Imb03,			/* 239.255.24.8 (XCS-IPM-03) XRT (controls) */
	XcsDg3Imb04,			/* 239.255.24.9 (XCS-DIO-03) XRT (controls) */
	HfxDg3Imb01,			/* 239.255.24.10 (XCS-IPM-03m) XRT (not connected) */
	HfxDg3Imb02,			/* 239.255.24.11 (XCS-DIO-03m) XRT (controls) */
	HxxDg1Cam,				/* 239.255.24.12 (XCS-YAG-1) XRT (controls) */
	HfxDg2Cam,				/* 239.255.24.13 (XCS-YAG-2) XRT (controls) */
	HfxDg3Cam,				/* 239.255.24.14 (XCS-YAG-3m) XRT (controls) */
	XcsDg3Cam,				/* 239.255.24.15 (XCS-YAG-3) XRT */
	HfxMonCam,				/* 239.255.24.16 (XCS-YAG-mono) XRT */
	HfxMonImb01,			/* 239.255.24.17 (XCS-IPM-mono) XRT */
	HfxMonImb02,			/* 239.255.24.18 (XCS-DIO-mono) XRT */
	HfxMonImb03,			/* 239.255.24.19 (XCS-DEC-mono) XRT */
	MecLasEm01,				/* 239.255.24.20 (MEC-LAS-EM-01) MEC Local */
	MecLasDio01,			/* 239.255.24.21 (MEC-TCTR-PIP-01) MEC Local */
	MecTcTrDio01,			/* 239.255.24.22 (MEC-TCTR-DI-01) MEC Local */
	MecXt2Ipm02,			/* 239.255.24.23 (MEC-XT2-IPM-02) MEC Local */
	MecXt2Ipm03,			/* 239.255.24.24 (MEC-XT2-IPM-03) MEC Local */
	MecHxmIpm01,			/* 239.255.24.25 (MEC-HXM-IPM-01) MEC XRT */
	GMD,					/* 239.255.24.26 SXR Local */
	CxiDg1Imb01,			/* 239.255.24.27 CXI Local */
	CxiDg2Imb01,			/* 239.255.24.28 CXI Local */
	CxiDg2Imb02,			/* 239.255.24.29 CXI Local */
	CxiDg4Imb01,			/* 239.255.24.30 CXI Local */
	CxiDg1Pim,				/* 239.255.24.31 CXI Local */
	CxiDg2Pim,				/* 239.255.24.32 CXI Local */
	CxiDg4Pim,				/* 239.255.24.33 CXI Local */
	XppMonPim0,				/* 239.255.24.34  XPP Local */
	XppMonPim1,				/* 239.255.24.35  XPP Local */
	XppSb2Ipm,				/* 239.255.24.36  XPP Local */
	XppSb3Ipm,				/* 239.255.24.37  XPP Local */
	XppSb3Pim,				/* 239.255.24.38  XPP Local */
	XppSb4Pim,				/* 239.255.24.39  XPP Local */
	XppEndstation0,			/* (XppEnds_Ipm0) 239.255.24.40  XPP Local */
	XppEndstation1,			/* (XppEnds_Ipm1) 239.255.24.41  XPP Local */
	MecXt2Pim02,			/* (MEC-XT2-PIM-02) 239.255.24.42  XPP Local */
	MecXt2Pim03,			/* (MEC-XT2-PIM-03) 239.255.24.43  XPP Local */
	CxiDg3Spec,				/* 239.255.24.44 */
	Nh2Sb1Ipm02,			/* 239.255.24.45  XPP + downstream */	 
	FeeSpec0,				/* 239.255.24.46 */
	SxrSpec0,				/* 239.255.24.47 */
	XppSpec0,				/* 239.255.24.48 */
} BLDMCGroup;

#endif
