## Example RTEMS startup script using the cexp
## shell.

## You may have to change BLD to something else
## everywhere it appears in this file
# Startup script for LCLS BLD production ioc-sys0-bd01

# For iocAdmin
setenv("LOCN","B005-2930")

# System Location:
epicsEnvSet("LOCA","SYS0")
epicsEnvSet("UNIT","BD01")
epicsEnvSet("FAC", "SYS0")
epicsEnvSet("NMBR","502")

setenv("IPADDR1","172.27.28.14",0)		# LCLS VME BD01 IOC ETH2 - ioc-sys0-bd01-fnet on LCLSFNET subnet
setenv("NETMASK1","255.255.252.0",0)

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

# =====================================================================
# Execute common fnet st.cmd
. "../st.fnetgeneric.lcls.cmd"

# execute generic part
. "../st.vmegeneric.cmd"

ld("bin/RTEMS-beatnik/BLDSender.obj")
bspExtVerbosity=0

## Configure 2nd NIC using lanIpBasic
lanIpSetup(getenv("IPADDR1"),getenv("NETMASK1"),0,0)
lanIpDebug=0

lsmod()

## Register all support components
dbLoadDatabase("dbd/BLDSender.dbd")
BLDSender_registerRecordDeviceDriver(pdbbase) 

# ==========================================================================================
# Let's dynamically load and register the RTEMS Spy Utility
# ==========================================================================================
# I hope to use ENV variables from "envPaths" with RTEMS someday!!

# Load RTEMS Spy Utility application object file:
# ld("bin/RTEMS-beatnik/rtemsUtilsSup.obj")

## Register components for RTEMS Spy Utility
# dbLoadDatabase("dbd/rtemsUtilsSup.dbd")
# rtemsUtilsSup_registerRecordDeviceDriver(pdbbase)
# =========================================================================================

# =========================================================================================
# Load up shortcuts to paths that can be used by the EPICS IOC Shell
# =========================================================================================
. startup/ioc-sys0-bd01/envPaths
. startup/ioc-sys0-bd01/cdCommands

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

#set fcom multicast prefix to mc-lcls-fcom for LCLS
epicsEnvSet ("FCOM_MC_PREFIX", "239.219.8.0")

#initialize FCOM now to work around RTEMS bug #2068
fcomInit(getenv("FCOM_MC_PREFIX",0),1000)

# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd01>")

epicsEnvSet("IOC_NAME","IOC=IOC:SYS0:BD01")

epicsEnvSet("AUTOSAVE_MACRO","P=IOC:SYS0:BD01:")
# ====================================================================
# Setup some additional environment variables
# ====================================================================
. "iocBoot/st.vmedb.cmd"

# END: Additional environment variables
# ====================================================================

# =====================================================================
# Turn Off BSP Verbosity
# =====================================================================
bspExtVerbosity=0

##################################################
# Initialize all hardware first                           #
###########################################################
# ======================================================
# Init PMC EVR: To support Timing System Event Receiver
# ======================================================
# In this case this EVR is running in a PC under linux

# =====================================================================
# Setup for EVR:
# =====================================================================
# Add ErConfigure for each EVR before iocInit.
#
#    VME:      ErConfigure(<instance>,<address>,<vector>,<level>,0)
#    PMC:      ErConfigure(<instance>,    0    ,    0   ,   0   ,1)
#    Embedded: ErConfigure(<instance>,    0    ,<vector>,<level>,2)
#
#    where instance = EVR instance, starting from 0, incrementing by 1
#                     for each subsequent card.  Only 1 EVR instance
#                    is allowed for Embedded EVRs.
#    and   address  = VME card address, starting from 0x300000,
#                     incrementing by 0x100000 for each subsequent card
#                     (0 for PMC and Embedded)
#    and   vector   = VME or Embedded interrupt vector.
#                    For VME, start from 0x60 and increment by 0x02 for
#                    each subsequent card.
#                     (0 for PMC)
#    and   level    = VME or Embedded interrupt level.
#                    For VME, set to 4.  Can be the same for all EVRs.
#                     (0 for PMC)
#    and   0        = VME
#       or 1        = PMC
#       or 2       = Embedded
#
# ======================================================================
# Debug interest level for EVR Driver
# ErDebug=100

#ErConfigure(0, 0, 0, 0, 1)
ErConfigure( 0,0x300000,0x60,4,0)       # VME EVR:SYS0:BD01
#evrTimeFlag=0

# Add evrInitialize (after ErConfigure) if a fiducial routine will be
# registered before iocInit driver initialization:
#evrInitialize()
# ======= EVR Setup Complete ============================================

########################################################################
# BEGIN: Load the record databases
#######################################################################
# =====================================================================
# Load database for autosave status
# =====================================================================
# dbLoadRecords("db/save_restoreStatus.db", "P=IOC:SYS0:BD01:")

# =====================================================================
# Set some facility specific MACROs for database instantiation below
# This is in effort to make IOCs Applications facility agnostic
# Some of the following variables may be defined in
# $IOC/<cpuName>/<epicsIOCName>/iocStartup.cmd
# =====================================================================
# Override the TOP variable set by envPaths:
# This is now past in via $IOC/<cpuName>/<epicsIOCName>/iocStartup.cmd

# ============================================
# Set MACROS for EVRs
# ============================================
# FAC = SYS0 ==> LCLS1
# FAC = SYS1 ==> FACET

epicsEnvSet("EVR_DEV1","EVR:SYS0:BD01")
epicsEnvSet("BSA_DEV1","BLD:SYS0:502")

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
dbLoadRecords("db/Pattern.db","IOC=IOC:SYS0:BD01,SYS=SYS0")

# Databases for the PMC EVR230
# Note the first instance of an EVR will inherit the unit number of the parent IOC.
# All additional EVR instances will have its unit number incremented by one.
# Hence,
# EVR device number one ==> EVR=EVR:B34:EV07
# EVR device number two ==> EVR=EVR:B34:EV08
dbLoadRecords("db/EvrPmc.db","EVR=EVR:SYS0:BD01,CRD=0,SYS=SYS0")
dbLoadRecords("db/PMC-trig.db","IOC=IOC:SYS0:BD01,LOCA=SYS0,UNIT=BD01,SYS=SYS0")

# Support for Beam Synchronous Acquisition (BSA)

# BSA Database for each data source from above
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=CHARGE,       EGU=nC")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=ENERGY,       EGU=MeV")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=POS_X,        EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=POS_Y,        EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=ANG_X,        EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=ANG_Y,        EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=BC2CHARGE,    EGU=Amps")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=BC2ENERGY,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=BC1CHARGE,    EGU=Amps")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=BC1ENERGY,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=UND_POS_X,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=UND_POS_Y,    EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=UND_ANG_X,    EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=UND_ANG_Y,    EGU=mrad")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=DMP_CHARGE,   EGU=Nel")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=XTCAV_AMP,    EGU=MV")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=PHOTONENERGY, EGU=eV")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=LTU450_POS_X, EGU=mm")
dbLoadRecords("db/Bsa.db","DEVICE=BLD:SYS0:502, ATRB=LTU250_POS_X, EGU=mm")

# =====================================================================
#Load Additional databases:
# =====================================================================
dbLoadRecords("db/access.db","DEV=BLD:SYS0:502:, MANAGE=IOCMANAGERS")

## Load record instances
# 5 = '2 second'

# Set the BLDSender data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.

dbLoadRecords("db/BLDMCast.db","LOCA=SYS0,NMBR=502, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/fcom_stats.db","LOCA=SYS0,NMBR=502, STAT_SCAN=5")

# Have a BLD listener running on this IOC and fill a waveform
# with the BLD data.
# We scan with event 146 (beam + .5Hz)
#
# NOTE: There must be one of the EVR:IOC:SYS0:BD01:EVENTxyCTRL
#       records holding the event number we use here and it
#       must have VME interrupts (.VME field) enabled.
#
#       Furthermore, you cannot use any event but only 
#       such ones for which an event record has been
#       instantiated with MRF ER device support -- this
#       is thanks to the great MRF software design, yeah!
#
# The erEvent record enables interrupts for an event
# the interrupt handler calls scanIoRequest(lists[event]) and
# there must be an event record registered on that list which
# then does post_event().
# (Well, the VME ISR firing 'event' could IMHO directly post_event(event)
# which would be faster, simpler and more flexible)
# 

dbLoadRecords("db/BLDMCastWfRecv.db","name=IOC:SYS0:BD01:BLDWAV, scan=Event, evnt=146, rarm=2")

# END: Loading the record databases
########################################################################

# =====================================================================
## Begin: Setup autosave/restore
# =====================================================================

# ============================================================
# If all PVs don't connect continue anyway
# ============================================================
# save_restoreSet_IncompleteSetsOk(1)

# ============================================================
# created save/restore backup files with date string
# useful for recovery.
# ============================================================
# save_restoreSet_DatedBackupFiles(1)

# ============================================================
# Where to find the list of PVs to save
# ============================================================
# set_requestfile_path("/data/autosave-req")

# ============================================================
# Where to write the save files that will be used to restore
# ============================================================
# set_savefile_path("/data", "autosave")

# ============================================================
# Prefix that is use to update save/restore status database
# records
# ============================================================
# save_restoreSet_status_prefix("IOC:SYS0:BD01:")

## Restore datasets
# set_pass0_restoreFile("info_settings.sav")
# set_pass1_restoreFile("info_settings.sav")

# set_pass0_restoreFile("info_positions.sav")
# set_pass1_restoreFile("info_positions.sav")

# BLD Multicast disabled in this ioc version to guard against 2 ioc's
# sending BLD packets to the same multicast address
BLD_MCAST_ENABLE=0

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

# Capture load addresses of all modules (essential for debugging if iocInit crashes...)

lsmod()

# =====================================================================
# End: Setup autosave/restore
# =====================================================================

# =============================================================
# Start EPICS IOC Process (i.e. all threads will start running)
# =============================================================

iocInit()

# =====================================================
# Turn on caPutLogging:
# Log values only on change to the iocLogServer:
# caPutLogInit(getenv("EPICS_CA_PUT_LOG_ADDR"))
# caPutLogShow(2)
# =====================================================

## Start any sequence programs
#seq(sncExample,"user=scondam")

## Start autosave process:
# Start the save_restore task 
# save changes on change, but no faster than every 30 seconds.
# Note: the last arg cannot be set to 0
# makeAutosaveFiles()
# create_monitor_set("info_settings.req",30,"")
# create_monitor_set("info_positions.req",30,"")

# Start rtems spy utility:
#iocshCmd("spy(2)")

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

# Capture load addresses of all modules (essential for debugging if iocInit crashes...)

lsmod()

# =====================================================================

