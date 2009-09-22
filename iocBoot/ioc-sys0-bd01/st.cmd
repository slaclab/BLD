#!../../bin/linux-x86/BLD

## You may have to change BLD to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/BLD.dbd"
BLD_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadRecords("db/xxx.db","user=pengsHost")

cd ${TOP}/iocBoot/${IOC}
iocInit

BLDMCastStart(1);

## Start any sequence programs
#seq sncxxx,"user=pengsHost"
