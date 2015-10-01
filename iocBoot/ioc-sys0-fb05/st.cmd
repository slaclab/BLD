#########################################
## So far lanIpBasic has no MC support ##
#########################################
ld("lanIpBasic_mve.obj")
#lanIpDebug=0
#padProtoDebug=0

#########################################
## So we just use second port and BSD  ##
#########################################

cd("../..")

errlogPrintf("Alert! BLD.obj no longer being built by the BLD ioc!\n")
# Load obj file
ld("bin/RTEMS-beatnik/BLD.obj")

fcomUtilSetIPADDR1("-fnet")

## Attach 2nd NIC to BSD stack:
#ifattach( "mve2", rtems_mve_attach, 0 )
## Configure 2nd NIC
#ifconf( "mve2", getenv("IPADDR1"), "255.255.252.0" )

## Configure 2nd NIC using lanIpBasic
lanIpSetup(getenv("IPADDR1"),"255.255.255.192",0,0)

fcomInit(fcomUtilGethostbyname("mc-lcls-fcom"),1000)


## You may have to change BLD to something else
## everywhere it appears in this file

##< envPaths
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-fb05>")

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

#setenv("EPICS_CA_AUTO_ADDR_LIST","NO")
#setenv("EPICS_CA_ADDR_LIST","172.27.11.54 172.27.9.76 172.27.9.77 172.27.9.78")

putenv ("EPICS_CA_MAX_ARRAY_BYTES=1000000")
#putenv ("EPICS_CA_SERVER_PORT=5068")

## Register all support components
dbLoadDatabase("dbd/BLD.dbd")
BLD_registerRecordDeviceDriver(pdbbase)
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
dbLoadRecords("db/IOC-SYS0-FB05.db")
# 5 = '2 second'

# Set the BLD data records (which are now deprecated,
# the BLDMcastWfRecv waveform should be used instead)
# to 'Passive' to effectively disable them.
dbLoadRecords("db/BLDMCastReceiverPhaseCavity.db","LOCA=501, DIAG_SCAN=I/O Intr, STAT_SCAN=5")
dbLoadRecords("db/BLDMCastReceiverImbs.db","DEVICE=BLD:SYS0:501")
dbLoadRecords("db/BLDMCastReceiver.db","DEVICE=BLD:SYS0:501")

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

# dbLoadRecords("db/BLDMCastWfRecv.db","name=IOC:SYS0:FB05:BLDWAV, scan=Event, evnt=146, rarm=2")

# ======================================================================
## Configure AutoSave and Restore
# ================================================================
cd("iocBoot")
## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:FB05:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

#cd("ioc-sys0-fb05")
cd(pathSubstitute("%H"))

set_requestfile_path("/data/autosave-req")
set_savefile_path("/data/autosave")
## ==============================================
## New method for autosave and restore
#  Decide what is saved by specifying it in the
#  EPICS databases
## =============================================
set_pass1_restoreFile("bldParams.sav")

#BLD_MCAST_DEBUG=2
#DELAY_FOR_CA=30

bld_hook_init()
iocInit()

## ===========================================================
## Start autosave routines to save our data
## ===========================================================
## Handle autosave 'commands' contained in loaded databases.
# change directory to ioc startup area

cd("/data/autosave-req")

## This is only good for iocShell
##makeAutosaveFiles()

epicsThreadSleep(1)
create_monitor_set("bldParams.req",30,0)

#BLDMCastStart(0, 0)
#BLDMCastStart(1, "172.27.225.21")

# One more sleep to allow mutex to be created before crashing on dbior()
epicsThreadSleep(5)
