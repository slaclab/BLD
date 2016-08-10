TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this

#DB += bsaBLDReceiverPhaseCavityEdef.db
#DB += bsaBLDReceiverImbEdef.db
#DB += bsaBLDReceiverGdetEdef.db
#DB += BLDMCastReceiverPcavs.db
#DB += BLDMCastReceiverImbs.db
#DB += BLDMCastReceiverGdets.db
#DB += BLDMCastReceiver.db

DB += bsaBLDEdef.db
DB += BLDMCast.db
DB += fcom_stats.db
DB += BLDMCastWfRecv.db
DB += dispersion.db
DB += simAo.db
DB += eBeamFcomSim.db
DB += eOrbitsFcom.db
DB += eOrbitsFcomSim.db

DB += evrFidTest.db

# EVR, BSA, Trigger related databases
#DB += IOC-SYS0-BD02evr.db
#DB += IOC-SYS0-BD02bsa.db
#DB += IOC-SYS0-BD02trig.db

DB += IOC-SYS0-BD01evr.db
DB += IOC-SYS0-BD01bsa.db
DB += IOC-SYS0-BD01trig.db

# Measurement and calculation for LCLS IN20 RF04 IOC-IN20-RF04
#DB += IOC-SYS0-BD02access.db

# ========================================
# Install timing/event related databases
# ========================================
DB_INSTALLS += $(EVR_MODULE)/db/EvrPmc.db
DB_INSTALLS += $(EVR_MODULE)/db/EvrPci.db
DB_INSTALLS += $(EVR_MODULE)/db/Pattern.db
DB_INSTALLS += $(EVR_MODULE)/db/PCI-trig.db
DB_INSTALLS += $(EVR_MODULE)/db/PMC-trig.db
DB_INSTALLS += $(EVR_MODULE)/db/VME-trig.db
DB_INSTALLS += $(EVR_MODULE)/db/VME-TTB-trig.db
DB_INSTALLS += $(EVR_MODULE)/db/Bsa.db
#DB_INSTALLS += $(EVR_MODULE)/db/evrEventCode.template

DB += IOC-SYS0-BD01access.db

# LCLS BSA and Timing
DB += lclsPattern.db

# ========================================================
# Install databases from iocAdmin package
# These databases support IOC Health and Monitoring
# ========================================================
# HardIOC
DB_INSTALLS += $(IOCADMIN)/db/iocAdminRTEMS.db

# SoftIOC
DB_INSTALLS += $(IOCADMIN)/db/iocAdminSoft.db

DB_INSTALLS += $(IOCADMIN)/db/iocAdminScanMon.db
DB_INSTALLS += $(IOCADMIN)/db/iocScanMonSum.db
DB_INSTALLS += $(IOCADMIN)/db/iocScanMon.db
DB_INSTALLS += $(IOCADMIN)/db/access.db

IOCRELEASE_DB += iocRelease.db

# Install Database to support reading the status of the autosave package:
DB_INSTALLS += $(AUTOSAVE)/db/save_restoreStatus.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>

include $(TOP)/configure/RULES

#----------------------------------------
#  ADD RULES AFTER THIS LINE
gen_trig_edl:
	create_edm_event_msi.py IOC-SYS0-BD02trig.substitutions
