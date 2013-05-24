TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC_RTEMS = BLD
PROD_IOC       = bldget bldrx

LIBRARY_IOC    = devBLDMCastWfRecv
# BLD.dbd will be created and installed
DBD += BLD.dbd
DBD += bldrx.dbd

# BLD.dbd will be made up from these files:
BLD_DBD += base.dbd

# Include dbd files from all support applications:
BLD_DBD += BLDMCast.dbd
BLD_DBD += autosaveSupport.dbd
BLD_DBD += evrSupport.dbd
BLD_DBD += devMrfEr.dbd
#BLD_DBD += dbRestore.dbd
BLD_DBD += iocAdmin.dbd
BLD_DBD += fcomUtil.dbd
BLD_DBD += devBusMapped.dbd
BLD_DBD += devBLDMCastWfRecv.dbd

# Add all the support libraries needed by this IOC
BLD_LIBS += evrSupport
#BLD_LIBS += generalTime
BLD_LIBS += devMrfEr
BLD_LIBS += mrfVme64x
BLD_LIBS += sSubRecord
#BLD_LIBS += restore
BLD_LIBS += autosave
BLD_LIBS += devIocStats
BLD_LIBS += fcomUtil
BLD_LIBS += fcom
BLD_LIBS += udpComm
BLD_LIBS += devBusMapped
BLD_LIBS += devBLDMCastWfRecv

# BLD_registerRecordDeviceDriver.cpp derives from BLD.dbd
BLD_SRCS += BLDMCast.c
BLD_SRCS += devBLDMCastStatus.c
BLD_SRCS += catest.c
BLD_SRCS += camon.c
BLD_SRCS += getBLD.c
BLD_SRCS += BLD_registerRecordDeviceDriver.cpp

BLD_SRCS += BLDMCastReceiver.c
BLD_SRCS += BLDMCastReceiverPhaseCavity.c
BLD_SRCS += devBLDMCastReceiverPhaseCavity.c


# Build the main IOC entry point on workstation OSs.
BLD_SRCS_DEFAULT += BLDMain.cpp
BLD_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#BLD_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
BLD_LIBS += $(EPICS_BASE_IOC_LIBS)

bldget_SRCS = bldget.c
bldget_LIBS = udpCommBSD Com

devBLDMCastWfRecv_SRCS = devBLDMCastWfRecv.c

bldrx_DBD += base.dbd
bldrx_DBD += devBLDMCastWfRecv.dbd

bldrx_SRCS_DEFAULT += BLDMain.cpp
bldrx_SRCS_RTEMS   += -nil-

bldrx_SRCS += bldrx_registerRecordDeviceDriver.cpp

bldrx_LIBS += devBLDMCastWfRecv
bldrx_LIBS_DEFAULT += udpCommBSD
bldrx_LIBS_RTEMS   += udpComm

bldrx_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

