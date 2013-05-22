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

DB += BLDMCast.db
DB += BLDMCastWfRecv.db
DB += IOC-SYS0-BD01.db
DB += IOC-SYS0-FB05.db

DB_INSTALLS += $(EVENT)/db/eventCodeSequenceAll.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

