# BLD MCAST Sender App RTEMS startup script for VME crate for the LCLS SYS0 BD02 IOC
# LCLS Multicast and BSA capable
#============================================================================================================================================================
# Author:
#       scondam: 10-Jun-2014:	Common scripts created for iocAdmin, autosave/restore etc.
#       scondam: 17-Jun-2014:   Split Sender and Receiver apps. ioc-sys0-BD02 is MCAST sender only.
#       scondam: 25-Jun-2014:   Removedbld_hook_init() as it is need only for BLDReceiver app.
#============================================================================================================================================================

# Startup script for LCLS BLD production ioc-sys0-BD02

# For iocAdmin
setenv("LOCN","B005-2930")
setenv("IOC_MACRO","IOC=IOC:SYS0:BD02")

# System Location:
setenv("LOCA","SYS0")
setenv("UNIT","BD02")
setenv("FAC", "SYS0")
setenv("NMBR","600")

# Set fcom multicast prefix to mc-lcls-fcom for LCLS Prod
# setenv( "FCOM_MC_PREFIX", "XXX.XX.XXX.X" ) should be set in host/startup.cmd

# setenv( "IP_EPICSCA", "XXX.XX.XXX.X" ) should be set in host/startup.cmd
# BLD MCAST traffic from photon side arrives with CA traffic on ETH0 network port
setenv("MCASTETHPORT","PROD_IPADDR0")
# BLD MCAST traffic to photon side sent on FNET using ETH2
setenv( "IP_BLD_SEND",		getenv("IP_FNET") )

# Set traditional FNET env vars
setenv( "IPADDR1",			getenv("IP_FNET") )
setenv( "NETMASK1",        "255.255.252.0" )

# SXR BLD multicast address
setenv( "BLDMCAST_DST_IP", "239.255.25.0"  )	# PROD
#setenv( "BLDMCAST_DST_IP", "239.255.24.254" )	# Test

# =====================================================================
# Execute common fnet st.cmd
. "../st.fnetgeneric.lcls.cmd"

# execute generic part
. "../st.vmegeneric.cmd"

# Load obj file
ld("../../bin/RTEMS-beatnik/BLDSenderSXR.obj")

# Load envPaths
. envPaths
chdir( "../.." )

# =====================================================================
# Turn Off BSP Verbosity
# =====================================================================
bspExtVerbosity=0

## Configure 2nd NIC using lanIpBasic
lanIpSetup(getenv("IPADDR1"),getenv("NETMASK1"),0,0)
lanIpDebug=0

lsmod()

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

#initialize FCOM now to work around RTEMS bug #2068
fcomInit(getenv("FCOM_MC_PREFIX",0),1000)

# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd02>")

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

## Register all support components
dbLoadDatabase("dbd/BLDSenderSXR.dbd")
BLDSenderSXR_registerRecordDeviceDriver(pdbbase)

###########################
# initialize all hardware #
###########################

bspExtVerbosity=0

# Init PMC EVR
ErConfigure(0, 0, 0, 0, 1)
#ErConfigure( 0,0x300000,0x60,4,0)       # VME EVR:SYS0:BD02

evrInitialize()

bspExtVerbosity = 1

###########################
## Load record instances ##
###########################

# Load EVR and Pattern databases
dbLoadRecords("db/IOC-SYS0-BD02evr.db","EVR=EVR:SYS0:BD02")	# EVR CARD 0

dbLoadRecords("db/lclsPattern.db","IOC=IOC:SYS0:BD02",0)

# bspExtMemProbe only durint init. clear this to avoid the lecture.
bspExtVerbosity = 0

# Set eBeam and eOrbits debug and enable variables
EBEAM_ENABLE=1
EORBITS_ENABLE=1
BLD_MCAST_ENABLE=1
BLD_MCAST_DEBUG=1
EORBITS_DEBUG=1
DEBUG_DRV_FCOM_RECV=1
DEBUG_DRV_FCOM_SEND=1
DEBUG_DEV_FCOM_RECV=1
DEBUG_DEV_FCOM_SEND=1
DEBUG_DEV_FCOM_SUB=1

# Load iocAdmin databases to support IOC Health and monitoring
# =====================================================================
dbLoadRecords("db/iocAdminRTEMS.db","IOC=IOC:SYS0:BD02",0)
dbLoadRecords("db/iocAdminScanMon.db","IOC=IOC:SYS0:BD02",0)

# The following database is a result of a python parser
# which looks at RELEASE_SITE and RELEASE to discover
# versions of software your IOC is referencing
# The python parser is part of iocAdmin
dbLoadRecords("db/iocRelease.db","IOC=IOC:SYS0:BD02",0)

# Load BSA database
dbLoadRecords("db/IOC-SYS0-BD02bsa.db",0)
# dbLoadRecords("db/IOC-SYS0-BD02bsa.db",0)

# Load access database
dbLoadRecords("db/IOC-SYS0-BD02access.db")

# Load trigger database
dbLoadRecords("db/IOC-SYS0-BD02trig.db")	# has only one EVRs' triggers

## Load record instances
# 5 = '2 second'

# Set the BLDSender data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.

dbLoadRecords("db/BLDMCast.db","LOCA=SYS0,NMBR=600, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/fcom_stats.db","LOCA=SYS0,NMBR=600, STAT_SCAN=5")
#dbLoadRecords("db/BLDMCastReceiverGdets.db","DEVICE=BLD:SYS0:600")

# Only load this on the production IOC
dbLoadRecords( "db/dispersion.db", "IOC=BLD:SYS0:600" )

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

dbLoadRecords("db/BLDMCastWfRecv.db","name=IOC:SYS0:BD02:BLDWAV, scan=Event, evnt=146, rarm=2")

# Load FCOM monitor databases
dbLoadRecords( "db/eOrbitsFcomSXR.db", "EC=40" )

# END: Loading the record databases
# =====================================================================
# Setup autosave/restore
# =====================================================================

#. "iocBoot/init_restore.cmd"

## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BD02:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

set_requestfile_path("/data/autosave-req")
set_savefile_path("/data/autosave")

set_pass0_restoreFile( "info_positions.sav" )
set_pass1_restoreFile( "info_settings.sav" )

# Capture load addresses of all modules (essential for debugging if iocInit crashes...)

lsmod()

# =====================================================================
# Start the EPICS IOC
# =====================================================================

iocInit()

# =====================================================
# Turn on caPutLogging:
# Log values only on change to the iocLogServer:
#caPutLogInit("172.27.8.31:7004")
#caPutLogShow(2)
# =====================================================

# Generate the autosave PV list (Takes a long time)
#chdir("/data/autosave-req")
#iocshCmd("makeAutosaveFiles()")

# Start autosave process:

. "iocBoot/start_restore.cmd"

epicsEnvShow()

nvramConfigShow()

bootConfigShow()

# dbl()

# Start rtems spy utility:
#iocshCmd("spy(2)")
