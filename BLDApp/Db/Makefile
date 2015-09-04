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

#DB += bsaBLDEdef.db
#DB += bsaBLDReceiverPhaseCavityEdef.db
#DB += bsaBLDReceiverImbEdef.db
#DB += bsaBLDReceiverGdetEdef.db

DB += BLDMCast.db
DB += BLDMCastWfRecv.db

# ========================================
# Install timing/event related databases
# ========================================
DB_INSTALLS += $(EVENT)/db/EvrPmc.db
DB_INSTALLS += $(EVENT)/db/EvrPci.db
DB_INSTALLS += $(EVENT)/db/Pattern.db
DB_INSTALLS += $(EVENT)/db/PCI-trig.db
DB_INSTALLS += $(EVENT)/db/PMC-trig.db
DB_INSTALLS += $(EVENT)/db/VME-trig.db
DB_INSTALLS += $(EVENT)/db/VME-TTB-trig.db
DB_INSTALLS += $(EVENT)/db/Bsa.db

# Measurement and calculation for LCLS IN20 RF04 IOC-IN20-RF04
#DB += IOC-SYS0-BD02access.db

# LCLS BSA and Timing
#DB += lclsPattern.db

# ========================================================
# Install databases from iocAdmin package
# These databases support IOC Health and Monitoring
# ========================================================
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
