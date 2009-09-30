## Example RTEMS startup script

IOC=pwd()
cd("../..")

# Load obj file
ld("bin/RTEMS-beatnik/BLD.obj")

## You may have to change BLD to something else
## everywhere it appears in this file

< envPaths
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bld1>")
putenv ("EPICS_CA_MAX_ARRAY_BYTES=8000000")
putenv ("EPICS_CA_SERVER_PORT=5068")

cd ${TOP}

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

## Load database for autosave/restore status pv's
dbLoadRecords("db/save_restoreStatus.db","P=IOC:SYS0:BD01:")

# ======================================================================
## Configure AutoSave and Restore
# ================================================================
cd ${TOP}/iocBoot
## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BD01:")
save_restoreSet_IncompleteSetsOk(1)
save_restoreSet_DatedBackupFiles(1)

set_requestfile_path("${TOP}/iocBoot/${IOC}/autosave-req")
set_savefile_path("${TOP}/iocBoot/${IOC}/autosave")
## ==============================================
## New method for autosave and restore
#  Decide what is saved by specifying it in the
#  EPICS databases
## =============================================
set_pass1_restoreFile("bldParams.sav")

cd ${TOP}/iocBoot/${IOC}
iocInit()

## ===========================================================
## Start autosave routines to save our data
## ===========================================================
## Handle autosave 'commands' contained in loaded databases.
# change directory to ioc startup area

cd autosave-req
makeAutosaveFiles()

epicsThreadSleep(1)
create_monitor_set("bldParams.req",30,"")

BLDMCastStart(0, "");

## Start any sequence programs
#seq sncxxx,"user=pengsHost"

