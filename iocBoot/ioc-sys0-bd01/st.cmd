#!../../bin/linux-x86/BLD

## You may have to change BLD to something else
## everywhere it appears in this file

< envPaths
# Set IOC Shell Prompt as well:
epicsEnvSet("IOCSH_PS1","ioc-sys0-bld1>")

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/BLD.dbd"
BLD_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/BLDMCast.db")

## Load database for autosave/restore status pv's
dbLoadRecords("db/save_restoreStatus.db","P=IOC:RPTC:ABCS01:")

# ======================================================================
## Configure AutoSave and Restore
# ================================================================
cd ${TOP}/iocBoot
## autosave/restore settings
save_restoreSet_status_prefix( "IOC:SYS0:BLD1:")
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
iocInit

## ===========================================================
## Start autosave routines to save our data
## ===========================================================
## Handle autosave 'commands' contained in loaded databases.
# change directory to ioc startup area

cd autosave-req
makeAutosaveFiles()

epicsThreadSleep(1)
create_monitor_set("bldParams.req",30,"")

BLDMCastStart(1);

## Start any sequence programs
#seq sncxxx,"user=pengsHost"
