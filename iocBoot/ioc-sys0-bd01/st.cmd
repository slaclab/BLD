# BLD MCAST Sender App RTEMS startup script for VME crate for the LCLS SYS0 BD01 IOC
# LCLS Multicast and BSA capable
#============================================================================================================================================================
# Author:
#       scondam: 10-Jun-2014:	Common scripts created for iocAdmin, autosave/restore etc.
#       scondam: 17-Jun-2014:   Split Sender and Receiver apps. ioc-sys0-bd01 is MCAST sender only.
#       scondam: 25-Jun-2014:   Removedbld_hook_init() as it is need only for BLDReceiver app.
#============================================================================================================================================================

# Startup script for LCLS BLD production ioc-sys0-bd01

# For iocAdmin
setenv("LOCN","B005-2930")
setenv("IOC_MACRO","IOC=IOC:SYS0:BD01")

# System Location:
epicsEnvSet("LOCA","SYS0")
epicsEnvSet("UNIT","BD01")
epicsEnvSet("FAC", "SYS0")
epicsEnvSet("NMBR","500")

setenv("IPADDR1","172.27.28.14",0)		# LCLS VME BD01 IOC ETH2 - ioc-sys0-bd01-fnet on LCLSFNET subnet
setenv("NETMASK1","255.255.252.0",0)
setenv("BLDMCAST_DST_IP", "239.255.24.0" )

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

# =====================================================================
# Execute common fnet st.cmd
. "../st.fnetgeneric.lcls.cmd"

# execute generic part
. "../st.vmegeneric.cmd"

# Load obj file
ld("bin/RTEMS-beatnik/BLDSender.obj")

# =====================================================================
# Turn Off BSP Verbosity
# =====================================================================
bspExtVerbosity=0

## Configure 2nd NIC using lanIpBasic
lanIpSetup(getenv("IPADDR1"),getenv("NETMASK1"),0,0)
lanIpDebug=0

lsmod()

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

#set fcom multicast prefix to mc-lcls-fcom for LCLS
epicsEnvSet ("FCOM_MC_PREFIX", "239.219.8.0")

#initialize FCOM now to work around RTEMS bug #2068
fcomInit(getenv("FCOM_MC_PREFIX",0),1000)

# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd01>")

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

## Register all support components
dbLoadDatabase("dbd/BLDSender.dbd")
BLDSender_registerRecordDeviceDriver(pdbbase)

###########################
# initialize all hardware #
###########################

bspExtVerbosity=0

# Init PMC EVR
ErConfigure(0, 0, 0, 0, 1)
#ErConfigure( 0,0x300000,0x60,4,0)       # VME EVR:SYS0:BD01

evrInitialize()

bspExtVerbosity = 1

###########################
## Load record instances ##
###########################

# Load EVR and Pattern databases
dbLoadRecords("db/IOC-SYS0-BD01evr.db","EVR=EVR:SYS0:BD01")	# EVR CARD 0

dbLoadRecords("db/lclsPattern.db","IOC=IOC:SYS0:BD01",0)

# bspExtMemProbe only durint init. clear this to avoid the lecture.
bspExtVerbosity = 0

# Load standard databases
. "iocBoot/st.vmedb.cmd"

# Load BSA database
dbLoadRecords("db/IOC-SYS0-BD01bsa.db",0)

# Load access database
dbLoadRecords("db/IOC-SYS0-BD01access.db")

# Load trigger database
dbLoadRecords("db/IOC-SYS0-BD01trig.db")	# has only one EVRs' triggers

## Load record instances
# 5 = '2 second'

# Set the BLDSender data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.

dbLoadRecords("db/BLDMCast.db","LOCA=SYS0,NMBR=500, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/fcom_stats.db","LOCA=SYS0,NMBR=500, STAT_SCAN=5")

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
# =====================================================================
# Setup autosave/restore
# =====================================================================

#. "iocBoot/init_restore.cmd"

cd("iocBoot")
## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BD01:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

cd(pathSubstitute("%H"))

set_requestfile_path("/data/autosave-req")
set_savefile_path("/data/autosave")

set_pass1_restoreFile("info_positions.sav")
set_pass1_restoreFile("info_settings.sav")

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

# Capture load addresses of all modules (essential for debugging if iocInit crashes...)

lsmod()

# =====================================================================
# Start the EPICS IOC
# =====================================================================

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

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

# One more sleep to allow mutex to be created before crashing on dbior()
epicsThreadSleep(5)

#BLDMCastStart(0, 0)
#BLDMCastStart(1, "172.27.225.21")

