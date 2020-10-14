TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

# Production flags
ifeq '$(OS_CLASS)' 'Linux'
USR_CPPFLAGS += -I$(EPICS_MODULES)/evrClient/$(EVRCLIENT_MODULE_VERSION)/include
USR_LDFLAGS  += -L$(EPICS_MODULES)/evrClient/$(EVRCLIENT_MODULE_VERSION)/lib/$(T_A)
USR_LDFLAGS  += -Wl,-rpath,$(EPICS_MODULES)/evrClient/$(EVRCLIENT_MODULE_VERSION)/lib/$(T_A)
else
USR_CPPFLAGS += -I$(EPICS_MODULES)/event/$(EVENT_MODULE_VERSION)/include
USR_LDFLAGS  += -L$(EPICS_MODULES)/event/$(EVENT_MODULE_VERSION)/lib/$(T_A)
USR_LDFLAGS  += -Wl,-rpath,$(EPICS_MODULES)/event/$(EVENT_MODULE_VERSION)/lib/$(T_A)
endif

#=============================
# Build an IOC support library

# Comment this line to build a host-ioc; you'd need to exclude
# several facilities (evr, micro, ...) for this to work.

#LIBRARY_IOC += devBLDMCastWfRecv
#LIBRARY_IOC	+= bldSndrSupport
#LIBRARY_IOC	+= bldRcvrSupport

# BLD*.dbd will be installed
DBD += devBLDRcvr.dbd
DBD += devBLDSndr.dbd


#=============================
# The following are compiled and added to the Support library for devBLDMCastWfRecv
#devBLDMCastWfRecv_SRCS = devBLDMCastWfRecv.c


# build an ioc application for Multicast Receiver only
# or Multicast Sender only or Receiver and Sender.
PROD_IOC      += BLDSender
PROD_IOC      += BLDReceiver
PROD_IOC      += BLDSenderSXR

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
BLDSender_DBD += devBLDRcvr.dbd
BLDSender_DBD += devBLDSndr.dbd
BLDSender_DBD += devBLDMCastWfRecv.dbd

# Add all the support libraries needed by this IOC for BLDSender app
BLDSender_LIBS += autosave
BLDSender_LIBS += devIocStats
BLDSender_LIBS += diagTimer
#BLDSender_LIBS += devBLDMCastWfRecv
BLDSender_LIBS += fcomIocshSupport
BLDSender_LIBS += fcomUtil
BLDSender_LIBS += fcom
BLDSender_LIBS += evrSupport
BLDSender_LIBS += devMrfEr
BLDSender_LIBS_RTEMS += mrfVme64x
BLDSender_LIBS_Linux += drvMrf
#BLDSender_LIBS_Linux += evrma
BLDSender_LIBS += devBusMapped
BLDSender_LIBS_RTEMS += udpComm
BLDSender_LIBS_Linux += udpCommBSD
BLDSender_LIBS += $(EPICS_BASE_IOC_LIBS)

# BLDSender_registerRecordDeviceDriver.cpp derives from BLDSender.dbd
BLDSender_SRCS += BLDMCast.c
BLDSender_SRCS += multicast_eOrbits.c
BLDSender_SRCS += devBLDMCastStatus.c
BLDSender_SRCS += devBLDMCastWfRecv.c
BLDSender_SRCS += devBLDScalars.c
#BLDSender_SRCS += catest.c
#BLDSender_SRCS += camon.c
#BLDSender_SRCS += getBLD.c

# Include Multicast Receiver support
BLDSender_SRCS += BLDMCastReceiver.c
BLDSender_SRCS += BLDMCastReceiverGdet.c
BLDSender_SRCS += BLDMCastReceiverImb.c
BLDSender_SRCS += BLDMCastReceiverPhaseCavity.c
BLDSender_SRCS += devBLDMCastReceiver.c

# Build the main IOC entry point on workstation OSs.
BLDSender_SRCS += BLDSender_registerRecordDeviceDriver.cpp
BLDSender_SRCS_DEFAULT += BLDMain.cpp
BLDSender_SRCS_vxWorks += -nil-

#=============================
# BLDSenderSXR test
#=============================

# BLDSenderSXR.dbd will be created and installed
DBD += BLDSenderSXR.dbd

# BLDSenderSXR.dbd will be made up from these files:
BLDSenderSXR_DBD += base.dbd
BLDSenderSXR_DBD += evrSupport.dbd
BLDSenderSXR_DBD += devMrfEr.dbd
BLDSenderSXR_DBD += autosaveSupport.dbd
BLDSenderSXR_DBD += iocAdmin.dbd
BLDSenderSXR_DBD += diagTimer.dbd
BLDSenderSXR_DBD += fcomIocshSupport.dbd
BLDSenderSXR_DBD += fcomUtil.dbd
BLDSenderSXR_DBD += devFcom.dbd
BLDSenderSXR_DBD += devBusMapped.dbd
# BLDSenderSXR_DBD += devBLDRcvr.dbd
BLDSenderSXR_DBD += devBLDSndr.dbd
BLDSenderSXR_DBD += devBLDMCastWfRecv.dbd

# Add all the support libraries needed by this IOC for BLDSenderSXR app
BLDSenderSXR_LIBS += autosave
BLDSenderSXR_LIBS += devIocStats
BLDSenderSXR_LIBS += diagTimer
#BLDSenderSXR_LIBS += devBLDMCastWfRecv
BLDSenderSXR_LIBS += fcomIocshSupport
BLDSenderSXR_LIBS += fcomUtil
BLDSenderSXR_LIBS += fcom
BLDSenderSXR_LIBS += evrSupport
BLDSenderSXR_LIBS += devMrfEr
BLDSenderSXR_LIBS_RTEMS += mrfVme64x
BLDSenderSXR_LIBS_Linux += drvMrf
#BLDSenderSXR_LIBS_Linux += evrma
BLDSenderSXR_LIBS += devBusMapped
BLDSenderSXR_LIBS_RTEMS += udpComm
BLDSenderSXR_LIBS_Linux += udpCommBSD
BLDSenderSXR_LIBS += $(EPICS_BASE_IOC_LIBS)


# BLDSenderSXR_registerRecordDeviceDriver.cpp derives from BLDSenderSXR.dbd
BLDSenderSXR_SRCS += BLDMCastSXR.c
BLDSenderSXR_SRCS += multicast_eOrbitsSXR.c
BLDSenderSXR_SRCS += devBLDMCastStatus.c
BLDSenderSXR_SRCS += devBLDMCastWfRecv.c
BLDSenderSXR_SRCS += devBLDScalars.c
#BLDSenderSXR_SRCS += catest.c
#BLDSenderSXR_SRCS += camon.c
#BLDSenderSXR_SRCS += getBLD.c

# Include Multicast Receiver support
# BLDSenderSXR_SRCS += BLDMCastReceiver.c
# BLDSenderSXR_SRCS += BLDMCastReceiverGdet.c
# BLDSenderSXR_SRCS += BLDMCastReceiverImb.c
# BLDSenderSXR_SRCS += BLDMCastReceiverPhaseCavity.c
# BLDSenderSXR_SRCS += devBLDMCastReceiver.c

# Build the main IOC entry point on workstation OSs.
BLDSenderSXR_SRCS += BLDSenderSXR_registerRecordDeviceDriver.cpp
BLDSenderSXR_SRCS_DEFAULT += BLDMain.cpp
BLDSenderSXR_SRCS_vxWorks += -nil-


#=============================
# BLDReceiver product
#=============================
# BLDReceiver.dbd will be created and installed
DBD += BLDReceiver.dbd

# BLDReceiver.dbd will be made up from these files:
BLDReceiver_DBD += base.dbd
BLDReceiver_DBD += evrSupport.dbd
BLDReceiver_DBD += devMrfEr.dbd
BLDReceiver_DBD += autosaveSupport.dbd
BLDReceiver_DBD += iocAdmin.dbd
BLDReceiver_DBD += devBusMapped.dbd
BLDReceiver_DBD += devBLDRcvr.dbd

#=============================

# Add all the support libraries needed by this IOC for BLDSender app
BLDReceiver_LIBS += autosave
BLDReceiver_LIBS += devIocStats
BLDReceiver_LIBS += evrSupport
BLDReceiver_LIBS += devMrfEr
BLDReceiver_LIBS_RTEMS += mrfVme64x
BLDReceiver_LIBS_Linux += drvMrf
#BLDReceiver_LIBS_Linux += evrma
BLDReceiver_LIBS += devBusMapped
BLDReceiver_LIBS += $(EPICS_BASE_IOC_LIBS)


# Include Multicast Receiver support
BLDReceiver_SRCS += BLDMCastReceiver.c
BLDReceiver_SRCS += BLDMCastReceiverGdet.c
BLDReceiver_SRCS += BLDMCastReceiverImb.c
BLDReceiver_SRCS += BLDMCastReceiverPhaseCavity.c
BLDReceiver_SRCS += devBLDMCastReceiver.c

# Build the main IOC entry point on workstation OSs.
BLDReceiver_SRCS += BLDReceiver_registerRecordDeviceDriver.cpp
BLDReceiver_SRCS_DEFAULT += BLDMain.cpp

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
