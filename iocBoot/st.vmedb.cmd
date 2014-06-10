###########################
## Load record instances ##
###########################

# ==========================================================================================
# Let's dynamically load the RTEMS Spy Utility
# ==========================================================================================
# I hope to use ENV variables from "envPaths" with RTEMS someday!!

# Load RTEMS Spy Utility application object file:
# ld("bin/RTEMS-beatnik/rtemsUtilsSup.obj")

## Register components for RTEMS Spy Utility
# dbLoadDatabase("dbd/rtemsUtilsSup.dbd")
# rtemsUtilsSup_registerRecordDeviceDriver(pdbbase)
# =========================================================================================

# For iocAdmin
epicsEnvSet("ENGINEER","Shantha Condamoor")
epicsEnvSet("LOCATION",getenv("LOCN"),1)
epicsEnvSet("LOCA_NAME",getenv("LOCAs"),1)
epicsEnvSet("IOC_UNIT",getenv("UNITs"),1)

########################################################################
# BEGIN: Load the record databases
#######################################################################
# Load iocAdmin databases to support IOC Health and monitoring
# =====================================================================
dbLoadRecords("db/iocAdminRTEMS.db",getenv("IOC_MACRO"),0)
#dbLoadRecords("db/iocAdminScanMon.db",getenv("IOC_MACRO"),0)

# The following database is a result of a python parser
# which looks at RELEASE_SITE and RELEASE to discover
# versions of software your IOC is referencing
# The python parser is part of iocAdmin
dbLoadRecords("db/iocRelease.db",getenv("IOC_MACRO"),0)

# =====================================================================
# Load database for autosave status
# =====================================================================
dbLoadRecords("db/save_restoreStatus.db", getenv("AUTOSAVE_MACRO"),0)

