# BLD MCAST Sender App RTEMS startup script for VME crate for the LCLS B34 BD01 IOC
# LCLS Multicast and BSA capable
#=============================================================================================
# Author:
#       scondam: 10-Jun-2014:	Common scripts created for iocAdmin, autosave/restore etc.
#=============================================================================================

# Startup script for LCLS BLD development

# For iocAdmin
setenv("LOCN","B34-R253")
setenv("IOC_MACRO","IOC=IOC:B34:BD01")

# System Location:
setenv("LOCA","B34")
setenv("UNIT","BD01")
setenv("FAC", "B34")
setenv("NMBR","504")

# Set fcom multicast prefix to mc-b034-fcom for LCLS Dev
# setenv( "FCOM_MC_PREFIX", "XXX.XX.XXX.X" ) should be set in host/startup.cmd

# setenv( "IP_EPICSCA", "XXX.XX.XXX.X" ) should be set in host/startup.cmd
# BLD MCAST traffic from photon side arrives with CA traffic on ETH0 network port
setenv( "IP_BLD_RECV",		getenv("IP_EPICSCA") )

# setenv( "IP_FNET", "XXX.XX.XXX.X" ) should be set in host/startup.cmd
# BLD MCAST traffic to photon side sent on FNET using ETH2
setenv( "IP_BLD_SEND",		getenv("IP_FNET") )

# Set traditional FNET env vars
setenv( "IPADDR1",			getenv("IP_FNET") )
setenv( "NETMASK1",        "255.255.252.0" )

# BLD multicast address
#setenv( "BLDMCAST_DST_IP", "239.255.24.0"  )	# PROD
setenv( "BLDMCAST_DST_IP", "239.255.24.254" )	# Test

# =====================================================================
# Execute common fnet st.cmd
. "../st.fnetgeneric.b34.cmd"

# execute generic part
. "../st.vmegeneric.cmd"

# Allows gdb to attach to this target
#ld( "rtems-gdb-stub.obj" )
#rtems_gdb_start( 200, 0 )
#rtems_gdb_start(0,0)
bspExtVerbosity=0

# Load obj file
ld("../../bin/RTEMS-beatnik/BLDSenderSXR.obj")

# Load envPaths
. envPaths
chdir( "../.." )


## Configure 2nd NIC using lanIpBasic
lanIpSetup(getenv("IPADDR1"),getenv("NETMASK1"),0,0)
lanIpDebug=0

#lsmod()

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

#initialize FCOM now to work around RTEMS bug #2068
fcomInit(getenv("FCOM_MC_PREFIX",0),1000)

# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-b34-bd01>")

## Register all support components
dbLoadDatabase("dbd/BLDSender.dbd")
BLDSenderSXR_registerRecordDeviceDriver(pdbbase)

###########################
# initialize all hardware #
###########################

bspExtVerbosity=0
ErDebugLevel(3)

# Init VME EVR
#ErConfigure( 0, 0x300000, 0x60, 4, 0 )
# Init PMC EVR
ErConfigure( 0, 0, 0, 0, 1 )

evrInitialize()
ErDebugLevel(1)

bspExtVerbosity = 1

###########################
## Load record instances ##
###########################


# Set eBeam and eOrbits debug and enable variables
EBEAM_ENABLE=1
EORBITS_ENABLE=1
BLD_MCAST_ENABLE=1
BLD_MCAST_DEBUG= 2
EORBITS_DEBUG= 2
DEBUG_DRV_FCOM_RECV=2
DEBUG_DRV_FCOM_SEND=2
DEBUG_DEV_FCOM_RECV=2
DEBUG_DEV_FCOM_SEND=2
DEBUG_DEV_FCOM_SUB= 2
# Load iocAdmin databases to support IOC Health and monitoring
# =====================================================================
dbLoadRecords("db/iocAdminRTEMS.db","IOC=IOC:B34:BD01",0)
dbLoadRecords("db/iocAdminScanMon.db","IOC=IOC:B34:BD01",0)

# The following database is a result of a python parser
# which looks at RELEASE_SITE and RELEASE to discover
# versions of software your IOC is referencing
# The python parser is part of iocAdmin
dbLoadRecords("db/iocRelease.db","IOC=IOC:B34:BD01",0)

# ==========================================================

# =====================================================================
# Load database for autosave status
# =====================================================================
dbLoadRecords("db/save_restoreStatus.db", "P=IOC:B34:BD01:")

# Load EVR and Pattern databases
dbLoadRecords( "db/EvrPmc.db",   "EVR=EVR:B34:BD01,CRD=0,SYS=SYS0" )
dbLoadRecords( "db/Pattern.db",  "IOC=IOC:B34:BD01,SYS=SYS0" )
#dbLoadRecords( "db/PMC-trig.db", "IOC=IOC:B34:BD01,SYS=SYS0,LOCA=B34,UNIT=01" )
#dbLoadRecords( "db/VME-trig.db", "IOC=IOC:B34:BD01,SYS=SYS0,LOCA=B34,UNIT=01" )

# Load BSA database
# dbLoadRecords("db/IOC-B34-BD01bsa.db",0)

# Load access database
# dbLoadRecords("db/IOC-B34-BD01access.db")

## Load record instances
# 5 = '2 second'

# Set the BLDSender data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.

dbLoadRecords("db/BLDMCast.db","LOCA=B34,NMBR=504, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/fcom_stats.db","LOCA=B34,NMBR=504, STAT_SCAN=5")

# Load these only on the production IOC or in a development environment as they
# may confict w/ the production BLDSender IOC due to fixed PV names
dbLoadRecords( "db/dispersion.db" )
dbLoadRecords( "db/simAo.db", "PV=BEND:LTU0:125:BDES,EGU=GeV/c,VAL=13.5" )


dbLoadRecords("db/BLDMCastWfRecv.db","name=IOC:B34:BD01:BLDWAV, scan=Event, evnt=146, rarm=2")

# Load FCOM monitor databases
dbLoadRecords( "db/eOrbitsFcom.db", "EC=40" )

# Load FCOM simulation databases
# Only load in development environment to avoid conflicts w/ production FCOM traffix
dbLoadRecords( "db/eBeamFcomSim.db",   "EC=40" )
dbLoadRecords( "db/eOrbitsFcomSim.db", "EC=40" )

# END: Loading the record databases
# =====================================================================
# Setup autosave/restore
# =====================================================================

save_restoreSet_status_prefix( "IOC:B34:BD01:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(0)

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
#caPutLogInit(getenv("EPICS_CA_PUT_LOG_ADDR"))
#caPutLogShow(2)
# =====================================================

# Generate the autosave PV list
makeAutosaveFileFromDbInfo( "/data/autosave-req/info_positions.req", "autosaveFields_pass0" )
makeAutosaveFileFromDbInfo( "/data/autosave-req/info_settings.req",  "autosaveFields" )

create_monitor_set( "info_positions.req", 5, "" )
create_monitor_set( "info_settings.req", 5, "" )

#epicsEnvShow()

#nvramConfigShow()

#bootConfigShow()


#dbl()


