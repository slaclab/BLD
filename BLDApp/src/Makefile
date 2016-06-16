TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

# Production flags
USR_CPPFLAGS +=

#=============================
# Build an IOC support library

# Comment this line to build a host-ioc; you'd need to exclude
# several facilities (evr, micro, ...) for this to work.

LIBRARY_IOC		+= devBLDMCastWfRecv

# BLD*.dbd will be installed
DBD += devBLDSndr.dbd

#=============================
# The following are compiled and added to the Support library for devBLDMCastWfRecv
devBLDMCastWfRecv_SRCS = devBLDMCastWfRecv.c

# build an ioc application for Multicast Sender
# PROD_IOC = $(PROD_IOC_$(T_A))
PROD_IOC      += BLDSender
# PROD_IOC      += BLDReceiver
# PROD_IOC      += bldget
# PROD_IOC      += bldrx

#=============================
# BLDSender.dbd will be created and installed
DBD += BLDSender.dbd

# BLDSender.dbd will be made up from these files:
BLDSender_DBD += base.dbd
BLDSender_DBD += evrSupport.dbd
BLDSender_DBD += devMrfEr.dbd
BLDSender_DBD += autosaveSupport.dbd
BLDSender_DBD += iocAdmin.dbd
BLDSender_DBD += diagTimer.dbd
BLDSender_DBD += fcomIocshSupport.dbd
BLDSender_DBD += fcomUtil.dbd
BLDSender_DBD += devFcom.dbd
BLDSender_DBD += devBusMapped.dbd
BLDSender_DBD += devBLDSndr.dbd
BLDSender_DBD += devBLDMCastWfRecv.dbd

#=============================

# Add all the support libraries needed by this IOC for BLDSender app
BLDSender_LIBS += autosave
BLDSender_LIBS += devIocStats
BLDSender_LIBS += diagTimer
BLDSender_LIBS += devBLDMCastWfRecv
BLDSender_LIBS += fcomIocshSupport
BLDSender_LIBS += fcomUtil
BLDSender_LIBS += fcom
BLDSender_LIBS += evrSupport
BLDSender_LIBS += devMrfEr
BLDSender_LIBS_RTEMS += mrfVme64x
BLDSender_LIBS_Linux += drvMrf
BLDSender_LIBS_Linux += evrma
BLDSender_LIBS += devBusMapped
BLDSender_LIBS_RTEMS += udpComm
BLDSender_LIBS_Linux += udpCommBSD 
BLDSender_LIBS += $(EPICS_BASE_IOC_LIBS)

# BLDSender_registerRecordDeviceDriver.cpp derives from BLDSender.dbd
BLDSender_SRCS += BLDMCast.c
BLDSender_SRCS += multicast_eOrbits.c
BLDSender_SRCS += devBLDMCastStatus.c
#BLDSender_SRCS += catest.c
#BLDSender_SRCS += camon.c
#BLDSender_SRCS += getBLD.c

# Build the main IOC entry point on workstation OSs.
BLDSender_SRCS += BLDSender_registerRecordDeviceDriver.cpp
BLDSender_SRCS_DEFAULT += BLDMain.cpp
BLDSender_SRCS_vxWorks += -nil-

#=============================
# BLDReceiver product
#=============================
#DBD += BLDReceiver.dbd

# BLDReceiver.dbd will be made up from these files:
#BLDReceiver_DBD += base.dbd
#BLDReceiver_DBD += evrSupport.dbd
#BLDReceiver_DBD += devMrfEr.dbd
#BLDReceiver_DBD += autosave.dbd
#BLDReceiver_DBD += iocAdmin.dbd
#BLDReceiver_DBD += devBusMapped.dbd
#BLDReceiver_DBD += devBLDRcvr.dbd


#bldRcvrSupport_SRCS += BLDMCastReceiver.c
#bldRcvrSupport_SRCS += BLDMCastReceiverPhaseCavity.c
#bldRcvrSupport_SRCS += devBLDMCastReceiver.c
#bldRcvrSupport_SRCS += BLDMCastReceiverImb.c
#bldRcvrSupport_SRCS += BLDMCastReceiverGdet.c

# Add all the support libraries needed by this IOC for BLDReceiver app
#BLDReceiver_LIBS += evrSupport
#BLDReceiver_LIBS += devMrfEr
#BLDReceiver_LIBS += mrfVme64x
#BLDReceiver_LIBS += autosave
#BLDReceiver_LIBS += devIocStats
#BLDReceiver_LIBS += devBusMapped
#BLDReceiver_LIBS += bldRcvrSupport

#=============================
# bldget product
#=============================
# bldget_SRCS = bldget.c
# bldget_LIBS = udpCommBSD Com


#=============================
# bldrx product
#=============================
#bldrx_DBD += base.dbd
#bldrx_DBD += devBLDMCastWfRecv.dbd

#bldrx_SRCS += BLDMain.cpp
#bldrx_SRCS += devBLDMCastStatus.c
#bldrx_SRCS_RTEMS   += -nil-

#bldrx_SRCS += bldrx_registerRecordDeviceDriver.cpp

#bldrx_LIBS_DEFAULT += udpCommBSD
#bldrx_LIBS_RTEMS   += udpComm

#bldrx_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

