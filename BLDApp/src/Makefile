TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC_RTEMS = BLD
# BLD.dbd will be created and installed
DBD += BLD.dbd

# BLD.dbd will be made up from these files:
BLD_DBD += base.dbd

# Include dbd files from all support applications:
BLD_DBD += BLDMCast.dbd
BLD_DBD += autosaveSupport.dbd
BLD_DBD += evrSupport.dbd
BLD_DBD += devMrfEr.dbd
#BLD_DBD += dbRestore.dbd
BLD_DBD += iocAdmin.dbd

# Add all the support libraries needed by this IOC
BLD_LIBS += evrSupport
BLD_LIBS += sSubRecord
BLD_LIBS += generalTime
BLD_LIBS += devMrfEr
BLD_LIBS += mrfVme64x
#BLD_LIBS += restore
BLD_LIBS += autosave
BLD_LIBS += devIocStats

# BLD_registerRecordDeviceDriver.cpp derives from BLD.dbd
BLD_SRCS += BLDMCast.c
BLD_SRCS += catest.c
BLD_SRCS += camon.c
BLD_SRCS += BLD_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
BLD_SRCS_DEFAULT += BLDMain.cpp
BLD_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#BLD_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
BLD_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

