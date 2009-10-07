#########################################
## So far lanIpBasic has no MC support ##
#########################################
#ld("lanIpBasic.obj")
#lanIpSetup("172.27.225.21","255.255.255.192",0,0)
#lanIpDebug=0
#padProtoDebug=0

#########################################
## So we just use second port and BSD  ##
#########################################
ld("miscUtils.obj")

## Attach 2nd NIC to BSD stack:
ifattach( "mve2", rtems_mve_attach, 0 )

## Configure 2nd NIC
ifconf( "mve2", "172.27.225.21", "255.255.255.192" )

cd("../..")

# Load obj file
ld("bin/RTEMS-beatnik/BLD.obj")

## You may have to change BLD to something else
## everywhere it appears in this file

##< envPaths
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bd01>")

setenv("EPICS_CAS_INTF_ADDR_LIST","172.27.10.162")
setenv("EPICS_CAS_AUTO_BEACON_ADDR_LIST","NO")
setenv("EPICS_CAS_BEACON_ADDR_LIST","172.27.11.255")

#setenv("EPICS_CA_AUTO_ADDR_LIST","NO")
#setenv("EPICS_CA_ADDR_LIST","172.27.11.54 172.27.9.76 172.27.9.77 172.27.9.78")

#putenv ("EPICS_CA_MAX_ARRAY_BYTES=8000000")
#putenv ("EPICS_CA_SERVER_PORT=5068")

## Register all support components
dbLoadDatabase("dbd/BLD.dbd")
BLD_registerRecordDeviceDriver(pdbbase)

# Init PMC EVR
ErConfigure(0, 0, 0, 0, 1)

###########################
## Load record instances ##
###########################
dbLoadRecords("db/IOC-SYS0-BD01.db")
dbLoadRecords("db/BLDMCast.db")

# ======================================================================
## Configure AutoSave and Restore
# ================================================================
cd("iocBoot")
## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BD01:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

#cd("ioc-sys0-bd01")
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

