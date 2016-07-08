# Generic piece of startup script for BLD VME IOC

# ld("watchdog.obj")
# wdStart(20)

# Debugger stub daemon
# ld("rtems-gdb-stub.obj")
#rtems_gdb_start(0,0)

# Dedicated ethernet + network stack driver
ld("lanIpBasic_mve.obj")

# =====================================================================
