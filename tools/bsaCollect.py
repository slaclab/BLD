#!/usr/local/lcls/package/python/current/bin/python

import sys
from epics import caget
from array import array

if len(sys.argv) != 2:
    print "Usage: " + sys.argv[0] + " [edef #]"
    exit(-1)

edefNumber = sys.argv[1]
beamDevices=["BPMS:IN20:221:TMIT", # CHARGE
             "BPMS:LTU1:250:X", "BPMS:LTU1:450:X", # ENERGY
             "BPMS:LTU1:720:X", "BPMS:LTU1:720:Y", # LTU POS/ANG
             "BPMS:LTU1:730:X", "BPMS:LTU1:730:Y",
             "BPMS:LTU1:740:X", "BPMS:LTU1:740:Y",
             "BPMS:LTU1:750:X", "BPMS:LTU1:750:Y",
             "BLEN:LI24:886:BIMAX" # BLEN
             ]
bldDevices=["BPMS:IN20:221:TMIT", "BPMS:LTU1:250:X", "BLEN:LI24:886:BIMAX"]
#bldDevices=["BLD:SYS0:500:CHARGEHST" + edefNumber, "BLD:SYS0:500:]
beamValues = {}
bldValues = {}

print "Collecting data"
# BPM/BLEN/FF data
for device in beamDevices:
    bsaDevice = device + "HST" + edefNumber
    sys.stdout.write(bsaDevice + ": ")
    beamValues[device] = caget(bsaDevice)
    first = beamValues[device][0]
    second = beamValues[device][1]
    last = beamValues[device][2799]
    print "%f %f ... %f" % (first, second, last)

# BLD data
for device in bldDevices:
    bsaDevice = device + "HST" + edefNumber
    sys.stdout.write(bsaDevice + ": ")
    bldValues[device] = caget(bsaDevice)
    first = bldValues[device][0]
    second = bldValues[device][1]
    last = bldValues[device][2799]
    print "%f %f ... %f" % (first, second, last)

# non BSA BLD data
dspr1 = caget("BLD:SYS0:500:DSPR1")
f = open("./" + "BLD:SYS0:500:DSPR1.bld", "w")
a = array('d', [dspr1])
a.tofile(f)
f.close()

dspr2 = caget("BLD:SYS0:500:DSPR2")
f = open("./" + "BLD:SYS0:500:DSPR2.bld", "w")
a = array('d', [dspr2])
a.tofile(f)
f.close()

fmatrix = caget("BLD:SYS0:500:FMTRX")
f = open("./" + "BLD:SYS0:500:FMTRX.bld", "w")
a = array('d')
a.fromlist(fmatrix)
a.tofile(f)
f.close()

print "Saving data"
for device in beamDevices:
    bsaDevice = device + "HST" + edefNumber + ".bsa"
    f = open("./" + bsaDevice, "w")
    a=array('d')
    a.fromlist(beamValues[device])
    a.tofile(f)
    f.close()
    print "  Created file " + bsaDevice

for device in bldDevices:
    bsaDevice = device + "HST" + edefNumber + ".bld"
    f = open("./" + bsaDevice, "w")
    a=array('d')
    a.fromlist(bldValues[device])
    a.tofile(f)
    f.close()
    print "  Created file " + bsaDevice
