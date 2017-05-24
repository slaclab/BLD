# BLD Receiver App RTEMS startup script for VME crate for the LCLS SYS0 BD02 IOC
# LCLS Multicast and BSA capable
#============================================================================================================================================================
# Author:
#       scondam: 10-Jun-2014:	Common scripts created for iocAdmin, autosave/restore etc.
#		scondam: 31-Mar-2015:	Added PhaseCavityTest MCAST address for testing Bruch Hill's new BAT2 code
#============================================================================================================================================================

# IOC-IN20-RF01 startup script for LCLS LLRF production

# For iocAdmin
epicsEnvSet("ENGINEER","Shantha Condamoor")
epicsEnvSet("LOCATION",getenv("LOCN"))
setenv("LOCN","B005-2930")
setenv("IOC_MACRO","IOC=IOC:SYS0:BD02")

setenv("LOCAs","SYS0",1)
setenv("UNITs","BD02")

# for save-restore
setenv("AUTOSAVE_PREFIX","IOC:SYS0:BD02:")
setenv("AUTOSAVE_MACRO","P=IOC:SYS0:BD02:")

setenv("IPADDR1","172.27.29.100",0)		# LCLS VME BD02 IOC ETH2 - ioc-sys0-bd02-fnet on LCLSFNET subnet
setenv("NETMASK1","255.255.252.0",0)

# =====================================================================

# execute generic part
. "../st.vmegeneric.cmd"

# Load obj file
ld("../../bin/RTEMS-beatnik/BLDReceiver.obj")

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
padProtoDebug=0

lsmod()

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")
# MCAST traffic shared with CA traffic on ETH0 network port
epicsEnvSet("MCASTETHPORT","PROD_IPADDR0")

## Register all support components
dbLoadDatabase("dbd/BLDReceiver.dbd")
BLDReceiver_registerRecordDeviceDriver(pdbbase)

# hack around the EPICS memory tester
free(malloc(1024*1024*32))

# From Till Straumann (for RTEMS 4.9.1 upgrade):
# This should set the VME chip into a mode where it
# surrenders the VME bus after every transaction.
# This means that the master has to rearbitrate for the bus for every cycle
# (which slows things down).
#
# The faster setting I had enabled would let the master hold on to the bus
# until some other master requests it.
# *(long*)(vmeTsi148RegBase + 0x234) &= ~ 0x18

# Add additional buffers; default amount not enough for some IOCs
# lanIpBscAddBufs( 2000 )

###########################
# initialize all hardware #
###########################

bspExtVerbosity=0

# Prod: Init PMC EVR
ErConfigure(0, 0, 0, 0, 1)			# PMC EVR:SYS0:BD02
# Lab: Init PMC EVR
#ErConfigure( 0,0x300000,0x60,4,0)   # VME EVR:SYS0:BD02

evrInitialize()
#bspExtVerbosity = 1

# Load EVR and Pattern databases
dbLoadRecords("db/IOC-SYS0-BD02evr.db","EVR=EVR:SYS0:BD02")	# EVR CARD 0

dbLoadRecords("db/lclsPattern.db","IOC=IOC:SYS0:BD02",0)

# bspExtMemProbe only durint init. clear this to avoid the lecture.
bspExtVerbosity = 0

###########################
## Load record instances ##
###########################
epicsEnvSet("IOC_MACRO","IOC=IOC:SYS0:BD02")
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd02:")

# Load standard databases
#. "iocBoot/st.vmedb.cmd"

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

# Load access database
dbLoadRecords("db/IOC-SYS0-BD02access.db")

# Load trigger database
dbLoadRecords("db/IOC-SYS0-BD02trig.db")	# has only one EVRs' triggers

# Load BLDReceiver databases
# scondam: 24-Jun-2014: EVG Timestamps now obtained via Fiducial processing - hence BLDMCastReceiver unneeded
#dbLoadRecords("db/BLDMCastReceiver.db","DEVICE=BLD:SYS0:500")
# PhaseCavityTest added
dbLoadRecords("db/BLDMCastReceiverPcavs.db","DEVICE=BLD:SYS0:500")
dbLoadRecords("db/BLDMCastReceiverImbs.db","DEVICE=BLD:SYS0:500")
dbLoadRecords("db/BLDMCastReceiverGdets.db","DEVICE=BLD:SYS0:500")

# END: Loading the record databases
# =====================================================================
# Setup autosave/restore
# =====================================================================

#. "iocBoot/init_restore.cmd"

## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BD01:")
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

bld_hook_init()
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

