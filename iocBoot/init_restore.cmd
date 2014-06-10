#==============================================================
#
#  Abs:  Autosave initalization (for a RTEMS IOC)
#
#  Name: init_restore.cmd
#
#  Facility:  LCLS LLRF Controls
#
#  Auth: 01-Aug-2012, Shantha Condamoor  (SCONDAM)
#  Rev:  dd-mmm-yyyy, Reviewer's Name (USERNAME)
#--------------------------------------------------------------
#  Mod:
#        dd-mmm-yyyy, Reviewer's Name (USERNAME)
#          comment
#
#==============================================================
#
# =====================================================================
## Begin: Setup autosave/restore for RTEMS-based IOCs
# =====================================================================
#save_restoreSet_NFSHost(getenv("NFS_FILE_SYSTEM"), iocData, "/data")

#save_restoreSet_Debug(1)

# Ok to restore a save set that had missing values (no CA connection to PV)?
# Ok to save a file if some CA connections are bad?

save_restoreSet_IncompleteSetsOk(1)

# In the restore operation, a copy of the save file will be written.  The
# file name can look like "auto_settings.sav.bu", and be overwritten every
# reboot, or it can look like "auto_settings.sav_020306-083522" (this is what
# is meant by a dated backup file) and every reboot will write a new copy.

save_restoreSet_DatedBackupFiles(1)

save_restoreSet_status_prefix(getenv("AUTOSAVE_PREFIX"))

# Setup location of request files
set_requestfile_path("/data/autosave-req")
set_requestfile_path("./")

# Setup location of saved files
set_savefile_path("/data","autosave")

# Restore datasets
# Specify what save files should be restored and when.
# Note: up to eight files can be specified for each pass.
set_pass0_restoreFile("info_positions.sav")
set_pass0_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_settings.sav")
set_pass1_restoreFile("info_positions.sav")

# Number of sequenced backup files (e.g., 'info_settings.sav0') to write
#save_restoreSet_NumSeqFiles(3)

# Time interval between sequenced backups
#save_restoreSet_SeqPeriodInSeconds(600)

# Time between failed .sav-file write and the retry.
#save_restoreSet_RetrySeconds(60)

# =====================================================================
# End: Setup autosave/restore
# =====================================================================
