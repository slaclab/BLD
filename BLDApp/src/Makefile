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
#BUILDFOR=_RTEMS

LIBRARY_IOC		+= devBLDMCastWfRecv
#LIBRARY_IOC$(BUILDFOR) += bldSndrSupport
#LIBRARY_IOC$(BUILDFOR)	+= bldRcvrSupport

# BLD*.dbd will be installed
#DBD += devBLDRcvr.dbd
DBD += devBLDSndr.dbd
#DBD += bldrx.dbd

#=============================
# The following are compiled and added to the Support library for devBLDMCastWfRecv
devBLDMCastWfRecv_SRCS = devBLDMCastWfRecv.c

# The following are compiled and added to the Support library for BLD Sender

# bldSndrSupport_LIBS_DEFAULT += -nil-
# bldSndrSupport_LIBS_RTEMS   += -nil-
#=============================

# build an ioc application for Multicast Sender
# PROD_IOC = $(PROD_IOC_$(T_A))

PROD_IOC = $(PROD_IOC_$(T_A))
PROD_IOC      += BLDSender
# PROD_IOC      += bldget bldrx

#=============================
# BLDSender.dbd will be created and installed
DBD += BLDSender.dbd

# BLDSender.dbd will be made up from these files:
BLDSender_DBD += base.dbd
BLDSender_DBD += evrSupport.dbd
BLDSender_DBD += devMrfEr.dbd
BLDSender_DBD += autosaveSupport.dbd
BLDSender_DBD += iocAdmin.dbd
BLDSender_DBD += fcomUtil.dbd
BLDSender_DBD += fcomIocshSupport.dbd
BLDSender_DBD_RTEMS += devBusMapped.dbd
BLDSender_DBD += devBLDSndr.dbd
BLDSender_DBD += devBLDMCastWfRecv.dbd

#=============================

# Add all the support libraries needed by this IOC for BLDSender app
BLDSender_LIBS += autosave
BLDSender_LIBS += devIocStats
BLDSender_LIBS += devBLDMCastWfRecv
BLDSender_LIBS += fcom
BLDSender_LIBS += fcomIocshSupport
BLDSender_LIBS += fcomUtil
BLDSender_LIBS += evrSupport
BLDSender_LIBS += devMrfEr
BLDSender_LIB_RTEMS  += mrfVme64x.dbd
BLDSender_LIBS_Linux += drvMrf
BLDSender_LIBS_RTEMS += devBusMapped
BLDSender_LIBS_RTEMS += udpComm
BLDSender_LIBS_Linux += udpCommBSD 
BLDSender_LIBS += $(EPICS_BASE_IOC_LIBS)


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

# bldget_SRCS = bldget.c
# bldget_LIBS = udpCommBSDBSD Com

#=============================

#bldrx_DBD += base.dbd

#bldrx_SRCS_DEFAULT += BLDMain.cpp
#bldrx_SRCS_RTEMS   += -nil-

#bldrx_SRCS += bldrx_registerRecordDeviceDriver.cpp

#bldrx_LIBS_DEFAULT += udpCommBSDBSD
#bldrx_LIBS_RTEMS   += udpCommBSD

#bldrx_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

