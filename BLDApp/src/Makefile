TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

#=============================
# These are for testing PCAV data coming from PCDS
# -DSIGNAL_TEST: do not receive BLD multicast data, generate fake data for testing!
# -DFB05_TEST  : used when running tests on FB05 - no
#USR_CPPFLAGS = -DxxxSIGNAL_TEST -DFB05_TEST
#USR_CFLAGS = -DxxxSIGNAL_TEST -DFB05_TEST

# Production flags
USR_CPPFLAGS =

#=============================
# Build an IOC support library

# Comment this line to build a host-ioc; you'd need to exclude
# several facilities (evr, micro, ...) for this to work.
ifndef TEST_ONLY
BUILDFOR=_RTEMS
endif

LIBRARY_IOC$(BUILDFOR)    += devBLDMCastWfRecv
#LIBRARY_IOC$(BUILDFOR)    += bldSndrSupport
LIBRARY_IOC$(BUILDFOR)    += bldRcvrSupport

# BLD*.dbd will be installed
DBD += devBLDRcvr.dbd
DBD += devBLDSndr.dbd
DBD += bldrx.dbd

#=============================
# The following are compiled and added to the Support library for devBLDMCastWfRecv
devBLDMCastWfRecv_SRCS = devBLDMCastWfRecv.c

# The following are compiled and added to the Support library for BLD Sender

# bldSndrSupport_LIBS_DEFAULT += -nil-
# bldSndrSupport_LIBS_RTEMS   += -nil-
#=============================

# The following are compiled and added to the Support library for BLD Receiver
bldRcvrSupport_SRCS += BLDMCastReceiver.c
bldRcvrSupport_SRCS += BLDMCastReceiverPhaseCavity.c
bldRcvrSupport_SRCS += devBLDMCastReceiverPhaseCavity.c
bldRcvrSupport_SRCS += BLDMCastReceiverImb.c
bldRcvrSupport_SRCS += devBLDMCastReceiverImb.c

bldRcvrSupport_LIBS_DEFAULT += -nil-
bldRcvrSupport_LIBS_RTEMS   += -nil-
#=============================

# build an ioc application for Multicast Receiver only or Multicast Sender only or Receiver and Sender.
# PROD_IOC = $(PROD_IOC_$(T_A))

PROD_IOC = $(PROD_IOC_$(T_A))
PROD_IOC$(BUILDFOR)-beatnik += BLDSender
PROD_IOC$(BUILDFOR)-beatnik += BLDReceiver
PROD_IOC       += bldget bldrx

#=============================
# BLDSender.dbd will be created and installed
DBD += BLDSender.dbd

# BLDSender.dbd will be made up from these files:
BLDSender_DBD += base.dbd
BLDSender_DBD += evrSupport.dbd
BLDSender_DBD += devMrfEr.dbd
BLDSender_DBD += autosave.dbd
BLDSender_DBD += iocAdmin.dbd
BLDSender_DBD += fcomUtil.dbd
BLDSender_DBD += devBusMapped.dbd
BLDSender_DBD += devBLDMCastWfRecv.dbd
BLDSender_DBD += devBLDSndr.dbd

# BLDReceiver.dbd will be created and installed
DBD += BLDReceiver.dbd

# BLDReceiver.dbd will be made up from these files:
BLDReceiver_DBD += base.dbd
BLDReceiver_DBD += evrSupport.dbd
BLDReceiver_DBD += devMrfEr.dbd
BLDReceiver_DBD += autosave.dbd
BLDReceiver_DBD += iocAdmin.dbd
BLDReceiver_DBD += devBusMapped.dbd
BLDReceiver_DBD += devBLDRcvr.dbd

#=============================

# Add all the support libraries needed by this IOC for BLDSender app
BLDSender_LIBS += evrSupport
BLDSender_LIBS += devMrfEr
BLDSender_LIBS += mrfVme64x
BLDSender_LIBS += autosave
BLDSender_LIBS += devIocStats
BLDSender_LIBS += devBusMapped
BLDSender_LIBS += devBLDMCastWfRecv
BLDSender_LIBS += udpComm 
BLDSender_LIBS += fcom
BLDSender_LIBS += fcomUtil
#BLDSender_LIBS += bldSndrSupport


# BLDSender_registerRecordDeviceDriver.cpp derives from BLDSender.dbd
BLDSender_SRCS += BLDMCast.c
BLDSender_SRCS += devBLDMCastStatus.c
BLDSender_SRCS += catest.c
BLDSender_SRCS += camon.c
BLDSender_SRCS += getBLD.c

# Build the main IOC entry point on workstation OSs.
BLDSender_SRCS += BLDSender_registerRecordDeviceDriver.cpp
BLDSender_SRCS_DEFAULT += BLDMain.cpp
BLDSender_SRCS_vxWorks += -nil-

#=============================

# Add all the support libraries needed by this IOC for BLDReceiver app
BLDReceiver_LIBS += evrSupport
BLDReceiver_LIBS += devMrfEr
BLDReceiver_LIBS += mrfVme64x
BLDReceiver_LIBS += autosave
BLDReceiver_LIBS += devIocStats
BLDReceiver_LIBS += devBusMapped
BLDReceiver_LIBS += bldRcvrSupport

# Build the main IOC entry point on workstation OSs.
BLDReceiver_SRCS += BLDReceiver_registerRecordDeviceDriver.cpp
BLDReceiver_SRCS_DEFAULT += BLDMain.cpp
BLDReceiver_SRCS_vxWorks += -nil-

#=============================

# Finally link to the EPICS Base libraries
BLDSender_LIBS += $(EPICS_BASE_IOC_LIBS)
BLDReceiver_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

bldget_SRCS = bldget.c
bldget_LIBS = udpCommBSD Com

#=============================

bldrx_DBD += base.dbd
bldrx_DBD += devBLDMCastWfRecv.dbd

bldrx_SRCS += devBLDMCastWfRecv.c
bldrx_SRCS_DEFAULT += BLDMain.cpp
bldrx_SRCS_RTEMS   += -nil-

bldrx_SRCS += bldrx_registerRecordDeviceDriver.cpp

bldrx_LIBS_DEFAULT += udpCommBSD
bldrx_LIBS_RTEMS   += udpComm

bldrx_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

