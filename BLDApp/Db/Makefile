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

DB += bsaBLDEdef.db
DB += bsaBLDReceiverPhaseCavityEdef.db
DB += bsaBLDReceiverImbEdef.db
DB += bsaBLDReceiverGdetEdef.db

DB += BLDMCast.db
DB += BLDMCastWfRecv.db
DB += BLDMCastReceiverPcavs.db
DB += BLDMCastReceiverImbs.db
DB += BLDMCastReceiverGdets.db
DB += BLDMCastReceiver.db

# BLD Receiver app databases for IOC-SYS0-BD02
# EVR, BSA, Trigger related databases
DB += IOC-SYS0-BD02evr.db
DB += IOC-SYS0-BD02bsa.db
DB += IOC-SYS0-BD02trig.db

DB += IOC-SYS0-BD01evr.db
DB += IOC-SYS0-BD01bsa.db
DB += IOC-SYS0-BD01trig.db

# Measurement and calculation for LCLS IN20 RF04 IOC-IN20-RF04
DB += IOC-SYS0-BD02access.db

DB += IOC-SYS0-BD01access.db

# LCLS BSA and Timing
DB += lclsPattern.db

# ========================================================
# Install databases from iocAdmin package
# These databases support IOC Health and Monitoring
# ========================================================
DB += $(IOCADMIN)/db/iocAdminRTEMS.db
IOCRELEASE_DB += iocRelease.db

# Install Database to support reading the status of the autosave package:
DB_INSTALLS += $(AUTOSAVE)/db/save_restoreStatus.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES

#----------------------------------------
#  ADD RULES AFTER THIS LINE
gen_trig_edl:
	create_edm_event_msi.py IOC-SYS0-BD02trig.substitutions
