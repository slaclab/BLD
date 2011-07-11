#!/usr/local/lcls/package/python/current/bin/python

import sys
from epics import caget
from array import array
import os
import re

#-------------------------------------------------------------------------------
#
# CHARGE
#
#-------------------------------------------------------------------------------
def compareCharge(bldCharge, bpmCharge):
    if len(bldCharge) != len(bpmCharge):
        return False

    for i in range(len(bldCharge)):
        if bldCharge[i] != bpmCharge[i]:
            return False
        
    return True

#-------------------------------------------------------------------------------
#
# ENERGY
#
#-------------------------------------------------------------------------------
def calculateSingleEnergy(bpmLtu250X, bpmLtu450X, dspr1, dspr2):
    energy = ((bpmLtu250X / (1000 * dspr1) + bpmLtu450X / (1000 * dspr2)) / 2 + 1) * 1000 * dspr1
    return energy

def calculateEnergy(bpmLtu250X, bpmLtu450X, dspr1, dspr2):
    bldEnergy=[]

    for i in range(len(bpmLtu250X)):
        energy = calculateSingleEnergy(bpmLtu250X[i], bpmLtu450X[i], dspr1, dspr2)
        bldEnergy.append(energy)

    return bldEnergy

def compareEnergy(bldEnergy, bpmLtu250X, bpmLtu450X, dspr1, dspr2):
    if (len(bpmLtu250X) != 2800) or (len(bpmLtu450X) != 2800):
        return False

    bldEnergyCalc = calculateEnergy(bpmLtu250X, bpmLtu450X, dspr1, dspr2)

    for i in range(len(bpmLtu250X)):
        if (bldEnergy[i] != bldEnergyCalc[i]):
            return False
    
    return True


#-------------------------------------------------------------------------------
#
# LTU POS/ANG
#
#-------------------------------------------------------------------------------
def calculateSingleLtu(bpmLtu720X, bpmLtu720Y, bpmLtu730X, bpmLtu730Y, bpmLtu740X,
                       bpmLtu740Y, bpmLtu750X, bpmLtu750Y, bldFmtrx):
    tempDA = [0, 0, 0, 0]
    i = 0;
    matrixOffset = 0;
    
    for i in range(4):
        acc = 0.0;
        acc += bldFmtrx[0+matrixOffset] * bpmLtu720X
        acc += bldFmtrx[1+matrixOffset] * bpmLtu720Y
        acc += bldFmtrx[2+matrixOffset] * bpmLtu730X
        acc += bldFmtrx[3+matrixOffset] * bpmLtu730Y
        acc += bldFmtrx[4+matrixOffset] * bpmLtu740X
        acc += bldFmtrx[5+matrixOffset] * bpmLtu740Y
        acc += bldFmtrx[6+matrixOffset] * bpmLtu750X
        acc += bldFmtrx[7+matrixOffset] * bpmLtu750Y
      
        tempDA[i] = acc;
        matrixOffset = matrixOffset + 8

    return tempDA

def calculateLtu(bpmLtu720X, bpmLtu720Y, bpmLtu730X, bpmLtu730Y, bpmLtu740X, bpmLtu740Y, bpmLtu750X, bpmLtu750Y, bldFmtrx):
    bldPosX=[]
    bldPosY=[]
    bldAngX=[]
    bldAngY=[]

    for i in range(len(bpmLtu720X)):
        posX, posY, angX, angY = calculateSingleLtu(bpmLtu720X[i],
                                                    bpmLtu720Y[i],
                                                    bpmLtu730X[i],
                                                    bpmLtu730Y[i],
                                                    bpmLtu740X[i],
                                                    bpmLtu740Y[i],
                                                    bpmLtu750X[i],
                                                    bpmLtu750Y[i],
                                                    bldFmtrx)
        bldPosX.append(posX)
        bldPosY.append(posY)
        bldAngX.append(angX)
        bldAngY.append(angY)

    return [bldPosX, bldPosY, bldAngX, bldAngY]
        
def compareLtu(bldPosX, bldPosY, bldAngX, bldAngY, bpmLtu720X, bpmLtu720Y, bpmLtu730X, bpmLtu730Y, bpmLtu740X, bpmLtu740Y, bpmLtu750X, bpmLtu750Y, bldFmtrx):
    if (len(bpmLtu720X) != 2800) or (len(bpmLtu720Y) != 2800) or (len(bpmLtu730X) != 2800) or (len(bpmLtu730Y) != 2800) or (len(bpmLtu740X) != 2800) or (len(bpmLtu740Y) != 2800) or (len(bpmLtu750X) != 2800) or (len(bpmLtu750Y) != 2800):
        return False

    bldPosXCalc, bldPosYCalc, bldAngXCalc, bldAngYCalc = calculateLtu(bpmLtu720X,
                                                                      bpmLtu720Y,
                                                                      bpmLtu730X,
                                                                      bpmLtu730Y,
                                                                      bpmLtu740X,
                                                                      bpmLtu740Y,
                                                                      bpmLtu750X,
                                                                      bpmLtu750Y, bldFmtrx)

    for i in range(len(bpmLtu720X)):
        if (bldPosX[i] != bldPosXCalc[i]) or (bldPosY[i] != bldPosYCalc[i]) or (bldAngX[i] != bldAngXCalc[i]) or (bldAngY[i] != bldAngYCalc[i]):
            return False
    
    return True

#-------------------------------------------------------------------------------
#
# BEAM LOSS
#
#-------------------------------------------------------------------------------
def compareBlm(bldBlm, blm):
    if len(bldBlm) != len(blm):
        return False

    for i in range(len(bldBlm)):
        if bldBlm[i] != blm[i]:
            return False
        
    return True

#=== MAIN ======================================================================

if len(sys.argv) != 2:
    print "Usage: " + sys.argv[0] + " [edef #]"
    exit(-1)

edefNumber = sys.argv[1]
beamDevices=["BPMS:IN20:221:TMIT", "BPMS:LTU1:250:X", "BPMS:LTU1:450:X", "BPMS:LTU1:720:X", "BPMS:LTU1:720:Y", "BPMS:LTU1:730:X", "BPMS:LTU1:730:Y", "BPMS:LTU1:740:X", "BPMS:LTU1:740:Y", "BPMS:LTU1:750:X", "BPMS:LTU1:750:Y"]
bldDevices=["BPMS:IN20:221:TMIT", "BPMS:LTU1:250:X"]

#bldDevices=["BLD:SYS0:500:CHARGEHST" + edefNumber, # CHARGE
#            "BLD:SYS0:500:ENERGYHST" + edefNumber, # ENERGY
#            "BLD:SYS0:500:POS_XHST" + edefNumber, # LTU POS_X
#            "BLD:SYS0:500:POS_YHST" + edefNumber, # LTU POS_Y
#            "BLD:SYS0:500:ANG_XHST" + edefNumber, # LTU ANG_X
#            "BLD:SYS0:500:ANG_YHST" + edefNumber, # LTU ANG_Y
#            "BLD:SYS0:500:BLENHST" + edefNumber, # ENERGY
#            ]

bldCaDevices=["BLD:SYS0:500:DSPR1", "BLD:SYS0:500:DSPR2", "BLD:SYS0:500:FMTRX"]
bldCaDevicesLen=[1, 1, 32]
beamValues = {}
bldValues = {}
bldCaValues = {}

print "Reading data"
for device in beamDevices:
    bsaFile = device + "HST" + edefNumber + ".bsa"
    if os.path.exists(bsaFile):
        f = open(bsaFile)
        a=array('d')
        a.fromfile(f, 2800)
        l=a.tolist()
        beamValues[device]=l
        f.close()
        print "%s: %f %f ... %f" % (bsaFile, a[0], a[1], a[2799])
    else:
        print "File " + bsaFile + " does not exist, exiting..."
        exit(-1)

for device in bldDevices:
    bsaFile = device + "HST" + edefNumber + ".bld"
    if os.path.exists(bsaFile):
        f = open(bsaFile)
        a=array('d')
        a.fromfile(f, 2800)
        l=a.tolist()
        bldValues[device]=l
        f.close()
        print "%s: %f %f ... %f" % (bsaFile, a[0], a[1], a[2799])
    else:
        print "File " + bsaFile + " does not exist, exiting..."
        exit(-1)

i = 0
for device in bldCaDevices:
    bsaFile = device + ".bld"
    if os.path.exists(bsaFile):
        f = open(bsaFile)
        a=array('d')
        a.fromfile(f, bldCaDevicesLen[i])
        l=a.tolist()
        bldCaValues[device]=l
        f.close()
        print "%s: %f" % (bsaFile, a[0])
        i = i+1
    else:
        print "File " + bsaFile + " does not exist, exiting..."
        exit(-1)


print ""
print "---> Comparing data <---"
sys.stdout.write("  Checking Charge: ")
if compareCharge(bldValues["BPMS:IN20:221:TMIT"], beamValues["BPMS:IN20:221:TMIT"]) == False:
# if compareCharge(bldValues["BLD:SYS0:500:CHARGEHST" + edefNumber], beamValues["BPMS:IN20:221:TMIT"]) == False:
    print "ERROR"
else:
    print "OK"

sys.stdout.write("  Checking Energy: ")
bldEnergy = calculateEnergy(beamValues["BPMS:LTU1:250:X"], beamValues["BPMS:LTU1:450:X"],
                 bldCaValues["BLD:SYS0:500:DSPR1"][0], bldCaValues["BLD:SYS0:500:DSPR2"][0])
if compareEnergy(bldEnergy, beamValues["BPMS:LTU1:250:X"], beamValues["BPMS:LTU1:450:X"],
                 bldCaValues["BLD:SYS0:500:DSPR1"][0], bldCaValues["BLD:SYS0:500:DSPR2"][0]) == False:
#if compareEnergy(bldValues["BLD:SYS0:500:ENERGYHST" + edefNumber],
#                 beamValues["BPMS:LTU1:250:X"], beamValues["BPMS:LTU1:450:X"],
#                 bldCaValues["BLD:SYS0:500:DSPR1"][0], bldCaValues["BLD:SYS0:500:DSPR2"][0]) == False:
    print "ERROR"
else:
    print "OK"


sys.stdout.write("  Checking LTU Pos/Ang: ")
bldPosX, bldPosY, bldAngX, bldAngY = calculateLtu(beamValues["BPMS:LTU1:720:X"],
                                                  beamValues["BPMS:LTU1:720:Y"],
                                                  beamValues["BPMS:LTU1:730:X"],
                                                  beamValues["BPMS:LTU1:730:Y"],
                                                  beamValues["BPMS:LTU1:740:X"],
                                                  beamValues["BPMS:LTU1:740:Y"],
                                                  beamValues["BPMS:LTU1:750:X"],
                                                  beamValues["BPMS:LTU1:750:Y"],
                                                  bldCaValues["BLD:SYS0:500:FMTRX"])
if compareLtu(bldPosX, bldPosY, bldAngX, bldAngY, beamValues["BPMS:LTU1:720:X"],
              beamValues["BPMS:LTU1:720:Y"], beamValues["BPMS:LTU1:730:X"],
              beamValues["BPMS:LTU1:730:Y"], beamValues["BPMS:LTU1:740:X"],
              beamValues["BPMS:LTU1:740:Y"], beamValues["BPMS:LTU1:750:X"],
              beamValues["BPMS:LTU1:750:Y"], bldCaValues["BLD:SYS0:500:FMTRX"]) == False:
#if compareLtu(bldValues["BLD:SYS0:500:POS_XHST" + edefNumber],
#              bldValues["BLD:SYS0:500:POS_YHST" + edefNumber],
#              bldValues["BLD:SYS0:500:ANG_XHST" + edefNumber],
#              bldValues["BLD:SYS0:500:ANG_YHST" + edefNumber],
#              beamValues["BPMS:LTU1:720:X"],
#              beamValues["BPMS:LTU1:720:Y"], beamValues["BPMS:LTU1:730:X"],
#              beamValues["BPMS:LTU1:730:Y"], beamValues["BPMS:LTU1:740:X"],
#              beamValues["BPMS:LTU1:740:Y"], beamValues["BPMS:LTU1:750:X"],
#              beamValues["BPMS:LTU1:750:Y"], bldCaValues["BLD:SYS0:500:FMTRX"]) == False:
    print "ERROR"
else:
    print "OK"


sys.stdout.write("  Checking BLEN: ")
if compareCharge(bldValues["BPMS:IN20:221:TMIT"], beamValues["BPMS:IN20:221:TMIT"]) == False:
# if compareBlm(bldValues["BLD:SYS0:500:BLENHST" + edefNumber], beamValues["BLEN:LI24:886:BIMAX"]) == False:
    print "ERROR"
else:
    print "OK"

print "\nBSA is correct\n"    
