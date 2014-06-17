# BLD Receiver App RTEMS startup script for VME crate for the LCLS SYS0 BD01 IOC
# LCLS Multicast and BSA capable
#============================================================================================================================================================
# Author:
#       scondam: 10-Jun-2014:	Common scripts created for iocAdmin, autosave/restore etc.
#       scondam: 17-Jun-2014:   Split Sender and Receiver apps. ioc-sys0-bd01 is MCAST sender only.
#============================================================================================================================================================

# IOC-IN20-RF01 startup script for LCLS LLRF production

# For iocAdmin
setenv("LOCN","B005-2930")
setenv("IOC_MACRO","IOC=IOC:SYS0:BD01")

setenv("LOCAs","SYS0",1)
setenv("UNITs","BD01")

# for save-restore
setenv("AUTOSAVE_PREFIX","IOC:SYS0:BD01:")
setenv("AUTOSAVE_MACRO","P=IOC:SYS0:BD01:")

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

# Load IN20 RF01 VME IOC
ld("bin/RTEMS-beatnik/BLDSender.obj")

# Only set IPADDR1 if the caller had not provided a value
# getenv("NEW_LANIP") && (pre_ipaddr1 || fcomUtilSetIPADDR1("-fnet"))

lanIpSetup(getenv("IPADDR1"),getenv("NETMASK1"),0,0)
lanIpDebug=0
padProtoDebug=0

lsmod()

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES","1000000")

#set fcom multicast prefix to mc-lcls-fcom for LCLS
epicsEnvSet ("FCOM_MC_PREFIX", "239.219.8.0")

#initialize FCOM now to work around RTEMS bug #2068
fcomInit(getenv("FCOM_MC_PREFIX",0),1000)

## Register all support components
dbLoadDatabase("dbd/BLDSender.dbd")
BLDSender_registerRecordDeviceDriver(pdbbase)

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
ErConfigure(0, 0, 0, 0, 1)           # PMC EVR:SYS0:BD01
#ErConfigure( 0,0x300000,0x60,4,0)       # VME EVR:SYS0:BD01

evrInitialize()
bspExtVerbosity = 1

# Load EVR and Pattern databases
dbLoadRecords("db/IOC-SYS0-BD01evr.db","EVR=EVR:SYS0:BD01")	# EVR CARD 0

dbLoadRecords("db/lclsPattern.db","IOC=IOC:SYS0:BD01",0)

# bspExtMemProbe only durint init. clear this to avoid the lecture.
bspExtVerbosity = 0

###########################
## Load record instances ##
###########################
epicsEnvSet("IOC_MACRO","IOC=IOC:SYS0:BD01")
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd01:")

# Load standard databases
. "iocBoot/st.vmedb.cmd"

# Load BSA database
dbLoadRecords("db/IOC-SYS0-BD01bsa.db",0)

# Load access database
dbLoadRecords("db/IOC-SYS0-BD01access.db")

# Load trigger database
dbLoadRecords("db/IOC-SYS0-BD01trig.db")	# has only one EVRs' triggers

###########################
## Load record instances ##
###########################

dbLoadRecords("db/BLDMCast.db","LOCA=500, DIAG_SCAN=I/O Intr, STAT_SCAN=5")

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

. "iocBoot/init_restore.cmd"

# Capture load addresses of all modules (essential for debugging if iocInit crashes...)

lsmod()

# =====================================================================
# Start the EPICS IOC
# =====================================================================

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

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
#create_monitor_set("bldParams.req",30,0)

#BLDMCastStart(0, 0)
#BLDMCastStart(1, "172.27.225.21")

