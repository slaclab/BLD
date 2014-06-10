#==============================================================
#
#  Abs:  Start the autosave task (for a RTEMS IOC)
#
#  Name: start_restore.cmd
#
#  Side: load this script after iocInit
#
#       "In the lore that has built up around autosave, 
#        PV's that should be restored only before record
#        initialization have been termed "positions".
#        All other PV's have been termed "settings".
#
#        Thus, you might run across a file called
#        'auto_positions.req', and now you'll know that 
#        the parameters in this file are intended to be 
#        restored only during autosave's pass-0."
#           - from autosave documentation
#
#  Facility:  LCLS LLRF Controls
#
#  Auth: 01-Aug-2012, Shantha Condamoor  (SCONDAM)
#  Rev:  dd-mmm-yyyy, Reviewer's Name (USERNAME)
#--------------------------------------------------------------
#  Mod:
#        dd-mmm-yyyy, First Lastname  (USERNAME):
#          comment
#
#==============================================================
#
# Wait before building autosave files
epicsThreadSleep(1)

# Generate the autosave PV list
# Note we need change directory to 
# where we are saving the restore 
# request file.

# (Takes a long time)
#cd("/data/autosave-req")
#makeAutosaveFiles()

# Start the save_restore task 
# save changes on change, but no faster
# than every 5 seconds.
# Note: the last arg cannot be set to 0
create_monitor_set("info_settings.req",30,"")
create_monitor_set("info_positions.req",30,"")

save_restoreSet_Debug(0)

# =====================================================================
# End: Start autosave/restore
# =====================================================================
