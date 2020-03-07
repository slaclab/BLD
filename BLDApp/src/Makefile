TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application



# BLD*.dbd will be installed
DBD += devBLDRcvr.dbd
DBD += devBLDSndr.dbd


# build an ioc application for Multicast Receiver only
# or Multicast Sender only or Receiver and Sender.
PROD_IOC_RTEMS      += BLDSender
PROD_IOC_RTEMS      += BLDReceiver

#=============================
# BLDSender.dbd will be created and installed
DBD += BLDSender.dbd

# BLDSender.dbd will be made up from these files:
BLDSender_DBD += base.dbd
BLDSender_DBD += evrSupport.dbd
BLDSender_DBD += devMrfEr.dbd
BLDSender_DBD += bsaCore.dbd
BLDSender_DBD += asSupport.dbd
BLDSender_DBD += iocAdmin.dbd
BLDSender_DBD += diagTimer.dbd
BLDSender_DBD += fcomIocshSupport.dbd
BLDSender_DBD += fcomUtil.dbd
BLDSender_DBD += devFcom.dbd
BLDSender_DBD += devBusMapped.dbd
BLDSender_DBD += devBLDRcvr.dbd
BLDSender_DBD += devBLDSndr.dbd
BLDSender_DBD += devBLDMCastWfRecv.dbd
# For EPICS PVA:  Qsrv
BLDSender_DBD += PVAServerRegister.dbd
BLDSender_DBD += qsrv.dbd

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
BLDSender_LIBS += BsaCore
BLDSender_LIBS_RTEMS += mrfVme64x

BLDSender_LIBS += devBusMapped
BLDSender_LIBS_RTEMS += udpComm

# cexpsh
BLDSender_DBDS += $(CEXP_DBDS) 
BLDSender_LIBS += $(CEXP_LIBS)     
BLDSender_SYS_LIBS+=$(CEXP_SYS_LIBS)


# EPICS QSRV
BLDSender_LIBS += qsrv
BLDSender_LIBS += $(EPICS_BASE_PVA_CORE_LIBS) 

# EPICS BASE LIBS
BLDSender_LIBS += $(EPICS_BASE_IOC_LIBS)

# BLDSender_registerRecordDeviceDriver.cpp derives from BLDSender.dbd
BLDSender_SRCS += BLDMCast.c
BLDSender_SRCS += multicast_eOrbits.c
BLDSender_SRCS += devBLDMCastStatus.c
BLDSender_SRCS += devBLDMCastWfRecv.c
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
# BLDReceiver product
#=============================
# BLDReceiver.dbd will be created and installed
DBD += BLDReceiver.dbd

# BLDReceiver.dbd will be made up from these files:
BLDReceiver_DBD += base.dbd
BLDReceiver_DBD += evrSupport.dbd
BLDReceiver_DBD += devMrfEr.dbd
BLDReceiver_DBD += bsaCore.dbd
BLDReceiver_DBD += asSupport.dbd
BLDReceiver_DBD += iocAdmin.dbd
BLDReceiver_DBD += devBusMapped.dbd
BLDReceiver_DBD += devBLDRcvr.dbd

# For EPICS PVA:  Qsrv
BLDReceiver_DBD += PVAServerRegister.dbd
BLDReceiver_DBD += qsrv.dbd

#=============================

# Add all the support libraries needed by this IOC for BLDSender app
BLDReceiver_LIBS += autosave
BLDReceiver_LIBS += devIocStats
BLDReceiver_LIBS += evrSupport
BLDReceiver_LIBS += devMrfEr
BLDReceiver_LIBS += BsaCore
BLDReceiver_LIBS_RTEMS += mrfVme64x
BLDReceiver_LIBS += devBusMapped

# cexpsh
BLDReceiver_DBDS += $(CEXP_DBDS) 
BLDReceiver_LIBS += $(CEXP_LIBS)     
BLDReceiver_SYS_LIBS+=$(CEXP_SYS_LIBS)


# EPICS QSRV
BLDReceiver_LIBS += qsrv
BLDReceiver_LIBS += $(EPICS_BASE_PVA_CORE_LIBS)

# EPICS BASE LIBS
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

