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
epicsEnvSet("LOCATION",getenv("LOCN"))

########################################################################
# BEGIN: Load the record databases
#######################################################################
# Load iocAdmin databases to support IOC Health and monitoring
# =====================================================================
dbLoadRecords("db/iocAdminRTEMS.db",IOC=getenv("IOC_NAME"))
dbLoadRecords("db/iocAdminScanMon.db",IOC=getenv("IOC_NAME"))

# The following database is a result of a python parser
# which looks at RELEASE_SITE and RELEASE to discover
# versions of software your IOC is referencing
# The python parser is part of iocAdmin
dbLoadRecords("db/iocRelease.db",IOC=getenv("IOC_NAME"))




