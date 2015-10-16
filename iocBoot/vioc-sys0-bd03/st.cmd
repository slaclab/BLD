#!../../bin/linuxRT_glibc-x86_64/BLDSender

# Network configuration for this IOC
# 172.27.3.78   	LCLSIOC
# 172.27.29.101		LCLSFNET
# 172.27.225.23		LCLSBLD

# For iocAdmin
epicsEnvSet("LOCN","B005-2918")

epicsEnvSet("IPADDR1","172.27.29.101",0)	# LCLS LinuxRT BD03 ETH? - cpu-sys0-bd03-fnet on LCLSFNET subnet
epicsEnvSet("NETMASK1","255.255.252.0",0)

epicsEnvSet("EPICS_CAS_INTF_ADDR_LIST","172.27.3.78")
#epicsEnvSet("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
#??? epicsEnvSet("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")
epicsEnvSet("EPICS_CAS_AUTO_BEACON_ADDR_LIST","YES")

< envPaths
# =====================================================================
# Execute common fnet st.cmd
# < "../st.fnetgeneric.lcls.cmd"

# Set common fnet variables
epicsEnvSet("NETMASK1","255.255.252.0",0)

# Set the FCOM multicast prefix
# Production FCOM group is mc-lcls-fcom,  239.219.8.0 on  MCAST-LCLS-FCOM subnet
# epicsEnvSet("FCOM_MC_PREFIX", "mc-lcls-fcom",1)
epicsEnvSet("FCOM_MC_PREFIX", "239.219.8.0", 1)
# Development FCOM group is mc-b034-fcom, 239.219.248.0 on  MCAST-B034-FCOM subnet
# Development epicsEnvSet("FCOM_MC_PREFIX", "mc-b034-fcom",1)      #  on  MCAST-B034-FCOM subnet

# execute generic part
< "../st.linuxgeneric.cmd"

# ====================================================================
# Setup some additional environment variables
# ====================================================================
# Setup environment variables

# tag log messages with IOC name
# How to escape the "vioc-sys0-bd03" as the PERL program
# will try to replace it.
# So, uncomment the following and remove the backslash
epicsEnvSet("EPICS_IOC_LOG_CLIENT_INET","${IOC}")

# =====================================================================
# Set some facility specific MACROs for database instantiation below
# This is in effort to make IOCs Applications facility agnostic
# Some of the following variables may be defined in
# $IOC/<cpuName>/<epicsIOCName>/iocStartup.cmd
# =====================================================================
# Override the TOP variable set by envPaths:
# This is now past in via $IOC/<cpuName>/<epicsIOCName>/iocStartup.cmd
# epicsEnvSet(TOP,"${IOC_APP}")

# ============================================
# Set MACROS for EVRs
# ============================================
# FAC = SYS0 ==> LCLS1
# FAC = SYS1 ==> FACET

# System Location:
epicsEnvSet("LOCA","SYS0")
epicsEnvSet("UNIT","BD03")
epicsEnvSet("FAC", "SYS0")
epicsEnvSet("NMBR","500")

epicsEnvSet("EVR_DEV1","EVR:${LOCA}:${UNIT}")
epicsEnvSet("BSA_DEV1","BLD:${LOCA}:${NMBR}")
# ===========================================

# ========================================================
# Support Large Arrays/Waveforms; Number in Bytes
# Please calculate the size of the largest waveform
# that you support in your IOC.  Do not just copy numbers
# from other apps.  This will only lead to an exhaustion
# of resources and problems with your IOC.
# The default maximum size for a channel access array is
# 16K bytes.
# ========================================================
# Uncomment and set appropriate size for your application:
#epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "800000")
# =========================================================================================
# Load up shortcuts to paths that can be used by the EPICS IOC Shell
# =========================================================================================
# <  startup/vioc-sys0-bd03/envPaths
# < startup/vioc-sys0-bd03/cdCommands

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","vioc-sys0-bd03>")

epicsEnvSet("IOC_NAME","VIOC:${LOCA}:${UNIT}")

epicsEnvSet("AUTOSAVE_MACRO","VIOC:${LOCA}:${UNIT}:")
# ====================================================================
# Setup some additional environment variables
# ====================================================================
# For iocAdmin
epicsEnvSet("ENGINEER","Shantha Condamoor")
epicsEnvSet("LOCATION",getenv("LOCN"),1)

# END: Additional environment variables
# ====================================================================

# ====================================================
## Register all support components
dbLoadDatabase("dbd/BLDSender.dbd",0,0)
BLDSender_registerRecordDeviceDriver(pdbbase)
# ====================================================

###########################################################
# Initialize all hardware first                           #
###########################################################
# ======================================================
# Init PMC EVR: To support Timing System Event Receiver
# ======================================================
# In this case this EVR is running in a PC under linux

# =====================================================================
# Setup for EVR:
# =====================================================================
#Add ErConfigure for each EVR before iocInit.
#
#    VME:      ErConfigure(<instance>,<address>,<vector>,<level>,0) or
#              ErConfigureWithSlot(<instance>,<address>,<vector>,<level>,0,<starting slot>)
#    PMC:      ErConfigure(<instance>,    0    ,    0   ,   0   ,1)
#    Embedded: ErConfigure(<instance>,    0    ,<vector>,<level>,2)
#
#    PCI-E:    ErConfigure(<instance>,    0    ,    0   ,   0   ,4)
#
#    where instance = EVR instance, starting from 0, incrementing by 1
#                     for each subsequent card.  Only 1 EVR instance
#                     is allowed for Embedded EVRs.
#    and   address  = VME card address, starting from 0x300000,
#                     incrementing by 0x100000 for each subsequent card
#                     (0 for PMC and Embedded)
#    and   vector   = VME or Embedded interrupt vector.
#                     For VME, start from 0x60 and increment by 0x02 for
#                     each subsequent card.
#                     (0 for PMC)
#    and   level    = VME or Embedded interrupt level.
#                     For VME, set to 4.  Can be the same for all EVRs.
#                     (0 for PMC)
#    and   0        = VME
#       or 1        = PMC
#       or 2        = Embedded
#
#       or 4        = PCI-E
#
#    and starting slot = first VME slot to start looking for EVRs.
#                        Set to 0 if their is only one CPU in the crate.
#                        (0 for PMC)
# ======================================================================
# Debug interest level for EVR Driver
# ErDebugLevel(0)

var BLD_MCAST_DEBUG 1

# PMC-based EVR (EVR230)
# These are the most popular
ErConfigure(0, 0, 0, 0, 1)       # PMC EVR

# PCIe-based EVR (EVR300)
# For Industrial PCs, these desired.
#ErConfigure(0, 0, 0, 0, 4)

# ======= EVR Setup Complete ============================================

# Initialize FCOM
fcomInit( ${FCOM_MC_PREFIX}, 1000 )

##############################################################################
# BEGIN: Load the record databases
##############################################################################
# ============================================================================
# Load iocAdmin databases to support IOC Health and monitoring
# ============================================================================
# The MACRO IOCNAME should be defined via the IOCs top level, "iocStartup.cmd"
#  found in $IOC/<iocName>/<viocName>/iocStartup.cmd
# The name must according the SLAC ICD PV naming convention.
dbLoadRecords("db/iocAdminSoft.db","IOC=${IOC_NAME}")
dbLoadRecords("db/iocAdminScanMon.db","IOC=${IOC_NAME}")

# The following database is a result of a python parser
# which looks at RELEASE_SITE and RELEASE to discover
# versions of software your IOC is referencing
# The python parser is part of iocAdmin
dbLoadRecords("db/iocRelease.db","IOC=${IOC_NAME}")

# =====================================================================
# Load database for autosave status
# =====================================================================
dbLoadRecords("db/save_restoreStatus.db", "P=${IOC_NAME}:")

# ========================================================
# Load EVR Databases for the Timing system
# ===========================================================================
# Change the EVR and CRD MACROs to specify your EVR Device name and instance
# Also, change the following MACROs accordingly, LOCA; UNIT
# Change SYS MACRO to match your facility:
# SYS0 = LCLS 1
# SYS1 = FACET
# SYS6 = XTA
# ===========================================================================
dbLoadRecords("db/Pattern.db","IOC=${IOC_NAME},SYS=${FAC}")

# Databases for the PMC EVR230
dbLoadRecords("db/EvrPmc.db","EVR=${EVR_DEV1},CRD=0,SYS=${FAC}")
dbLoadRecords("db/PMC-trig.db","IOC=${IOC_NAME},LOCA=${LOCA},UNIT=${UNIT},SYS=${FAC}")

# Support for Beam Synchronous Acquisition (BSA)
# BSA Database for each data source from above
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=CHARGE,       EGU=nC")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=ENERGY,       EGU=MeV")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=POS_X,        EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=POS_Y,        EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=ANG_X,        EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=ANG_Y,        EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=BC2CHARGE,    EGU=Amps")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=BC2ENERGY,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=BC1CHARGE,    EGU=Amps")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=BC1ENERGY,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=UND_POS_X,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=UND_POS_Y,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=UND_ANG_X,    EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=UND_ANG_Y,    EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=DMP_CHARGE,   EGU=Nel")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=XTCAV_AMP,    EGU=MV")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=PHOTONENERGY, EGU=eV")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=LTU450_POS_X, EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=${BSA_DEV1}, ATRB=LTU250_POS_X, EGU=mm")

# ========================================================

# =====================================================================
#Load Additional databases:
# =====================================================================
## Load record instances

# Set the BLDSender data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.

# 5 = '2 second'
dbLoadRecords("db/BLDMCast.db","LOCA=${LOCA},NMBR=${NMBR}, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/fcom_stats.db","LOCA=${LOCA},NMBR=${NMBR}, STAT_SCAN=5")

# Have a BLD listener running on this IOC and fill a waveform
# with the BLD data.
# We scan with event 146 (beam + .5Hz)
#
# NOTE: There must be one of the erevent
#       records holding the event number we use here and it
#       must have VME interrupts (.VME field) enabled.
#
#       Furthermore, you cannot use any event but only 
#       such ones for which an event record has been
#       instantiated with MRF ER device support -- this
#       is thanks to the event module software design, yeah!
#
# The erEvent record enables interrupts for an event
# the interrupt handler calls scanIoRequest(lists[event]) and
# there must be an event record registered on that list which
# then does post_event().
# (Well, the VME ISR firing 'event' could IMHO directly post_event(event)
# which would be faster, simpler and more flexible)
# 

dbLoadRecords("db/BLDMCastWfRecv.db","name=VIOC:${LOCA}:${UNIT}:BLDWAV, scan=Event, evnt=146, rarm=2")

# END: Loading the record databases
########################################################################

# =====================================================================
## Begin: Setup autosave/restore
# =====================================================================

# ============================================================
# If all PVs don't connect continue anyway
# ============================================================
save_restoreSet_IncompleteSetsOk(1)

# ============================================================
# created save/restore backup files with date string
# useful for recovery.
# ============================================================
# save_restoreSet_DatedBackupFiles(1)

# ============================================================
# Where to find the list of PVs to save
# ============================================================
# Where "/data" is an NFS mount point setup when linuxRT target 
# boots up.
set_requestfile_path("/data/${IOC}/autosave-req")

# ============================================================
# Where to write the save files that will be used to restore
# ============================================================
set_savefile_path("/data/${IOC}/autosave")

# ============================================================
# Prefix that is use to update save/restore status database
# records
# ============================================================
save_restoreSet_UseStatusPVs(1)
save_restoreSet_status_prefix("${IOC_NAME}:")

## Restore datasets
set_pass0_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_settings.sav")

# =====================================================================
# End: Setup autosave/restore
# =====================================================================

# =====================================================================
# Channel Access Security:  
# This is required if you use caPutLog.
# Set access security file
# Load common LCLS Access Configuration File
< ${ACF_INIT}

# =============================================================
# Start EPICS IOC Process (i.e. all threads will start running)
# =============================================================
iocInit()

# =====================================================
# Turn on caPutLogging:
# Log values only on change to the iocLogServer:
# caPutLogInit("${EPICS_CA_PUT_LOG_ADDR}")
# caPutLogShow(2)
# =====================================================


## =========================================================================
## Start autosave routines to save our data
## =========================================================================
# optional, needed if the IOC takes a very long time to boot.
# epicsThreadSleep( 1.0)

# The following command makes the autosave request files 'info_settings.req',
# and 'info_positions.req', from information (info nodes) contained in all of
# the EPICS databases that have been loaded into this IOC.

makeAutosaveFileFromDbInfo( "/data/${IOC}/autosave-req/info_settings.req", "autosaveFields" )
create_monitor_set("info_settings.req",10,"")

# ===========================================================================

epicsThreadShowAll()

# ===========================================================================
# Setup Real-time priorities after iocInit for driver threads
# ===========================================================================
# system("/bin/su root -c `pwd`/rtPrioritySetup.cmd")

