# Generic piece of startup script for BLD VME IOC

pwd()
# /data now mapped by pre_st.cmd
nfsMountsShow()

# ld("watchdog.obj")
# wdStart(20)

# Debugger stub daemon
# ld("rtems-gdb-stub.obj")
#rtems_gdb_start(0,0)

# Dedicated ethernet + network stack driver
ld("lanIpBasic_mve.obj")

chdir("../../")
pwd()

# =====================================================================
