#!/usr/bin/python
# IBM_PROLOG_BEGIN_TAG
# This is an automatically generated prolog.
#
# $Source: src/tools/debug/sbe-debug.py $
#
# OpenPOWER sbe Project
#
# Contributors Listed Below - COPYRIGHT 2016
# [+] International Business Machines Corp.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#
# IBM_PROLOG_END_TAG
import os
import subprocess
import re
import random
import argparse
import sys
import binascii
err = False

syms = {};
if 'SBE_TOOLS_PATH' in os.environ:
    SBE_TOOLS_PATH = os.environ['SBE_TOOLS_PATH'];
else:
    print "SBE_TOOLS_PATH not defined"
    exit(1)

baseAddr = 0xfffe8000

def fillSymTable(sbeObjDir, target ):
    if (target == 'AWAN'):
        symFile = sbeObjDir + "/sim.sbe.syms"
    else:
        symFile = sbeObjDir + "/sbe.syms"
    f = open( symFile, 'r' )
    for line in f:
        words = line.split()
        if( len( words ) == 4 ):
            syms[words[3]] = [words[0], words[1]]

def getOffset( symbol ):
    symAddr = syms[ symbol][0]
    print "\n symAddress :", symAddr
    offset = int(symAddr, base = 16) - baseAddr;
    return hex(offset)

def collectTrace( sbeObjDir, target, node, proc ):
    offset = getOffset( 'g_pk_trace_buf' );
    len = "0x" + syms['g_pk_trace_buf'][1];
    cmd1 = ("p9_pibmem_dump_wrap.exe -quiet -start_byte " + \
            str(offset) +\
             " -num_of_byte " + len + " "
             " -n" + str(node) + " -p" + str(proc))
    cmd2 = sbeObjDir + "/ppe2fsp DumpPIBMEM sbetrace.bin "
    cmd3 = (sbeObjDir + "/fsp-trace -s " + sbeObjDir +\
                         "/sbeStringFile sbetrace.bin > "+\
                         "sbe_"+str(proc)+"_tracMERG")
    cmd4 = "mv DumpPIBMEM dumpPibMem_trace"
    print "\ncollecting trace with commands -\n"
    print "cmd1:", cmd1
    rc = os.system( cmd1 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd1, rc )
       return 1

    print "cmd2:", cmd2
    rc = os.system( cmd2 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd2, rc )
       return 1

    print "cmd3:", cmd3
    rc = os.system( cmd3 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd3, rc )
       return 1

    print "cmd4:", cmd4
    rc = os.system( cmd4 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd4, rc )
       return 1

def collectAttr( sbeObjDir, target, node, proc ):
    if (target == 'AWAN'):
        sbeImgFile = "p9n_10.sim.sbe_seeprom.bin"
    else:
        sbeImgFile = "sbe_seeprom.bin"
    offset = getOffset( 'G_sbe_attrs' );
    len = "0x" + syms['G_sbe_attrs'][1];
    cmd1 = ("p9_pibmem_dump_wrap.exe -quiet -start_byte " + \
            str(offset) +\
             " -num_of_byte " + len + " "
             " -n" + str(node) + " -p" + str(proc))
    cmd2 = "mv DumpPIBMEM sbeAttr.bin"
    # TODO via RTC 158861
    # For multi-node system we need to convert node/proc to absolute
    # proc number.
    cmd3 = ( sbeObjDir + "/p9_xip_tool " +\
             sbeObjDir + "/" + sbeImgFile + " -ifs attrdump sbeAttr.bin > "+\
             "sbe_"+str(proc)+"_attrs")
    print "\ncollecting attributes with commands -\n"
    print "cmd1:", cmd1
    rc = os.system( cmd1 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd1, rc )
       return 1

    print "cmd2:", cmd2
    rc = os.system( cmd2 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd2, rc )
       return 1

    print "cmd3:", cmd3
    rc = os.system( cmd3 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd3, rc )
       return 1

def ppeState( sbeObjDir, target, node, proc ):
    cmd1 = ("p9_ppe_state_wrap.exe -quiet -sbe -snapshot" +\
             " -n" + str(node) + " -p" + str(proc))
    print "cmd1:", cmd1
    rc = os.system( cmd1 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd1, rc )
       return 1

def sbeState( sbeObjDir, target, node, proc ):
    cmd1 = ("p9_pibms_reg_dump_wrap.exe -quiet" +\
             " -n" + str(node) + " -p" + str(proc))
    print "cmd1:", cmd1
    rc = os.system( cmd1 )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmd1, rc )
       return 1

def sbeStatus( node, proc ):
    cmd = ("getcfam pu 2809" +\
            " -n" + str(node) + " -p" + str(proc))
    print "cmd:", cmd
    output = os.popen(cmd).read()
    output = output.split()
    if ("ERROR:" in output):
      print "Error while getting the status register"
      print ' '.join(output)
    else:
      parsevalue(bin(int(output[6], 16)))

def parsevalue(iValue):
    sbeStates = {'0000' : 'SBE_STATE_UNKNOWN' , '0001' : 'SBE_STATE_IPLING' ,
                 '0010' : 'SBE_STATE_ISTEP', '0011' : 'SBE_STATE_MPIPL' ,
                 '0100' : 'SBE_STATE_RUNTIME' , '0101' : 'SBE_STATE_DMT' ,
                 '0110' : 'SBE_STATE_DUMP' , '0111' : 'SBE_STATE_FAILURE' ,
                 '1000' : 'SBE_STATE_QUIESCE' , '1001' : 'SBE_STATE_QUIESCE' ,
                 '1111' : 'SBE_INVALID_STATE'}

    tempVal = iValue[2:3]
    tempVal = 'True' if tempVal == '1' else 'False'
    print "SBE Booted           : %s" %(tempVal)

    tempVal = iValue[3:4]
    tempVal = 'True' if tempVal == '1' else 'False'
    print "Async FFDC           : %s" %(tempVal)

    tempVal = iValue[4:6]
    print "Reserver Bit [2:3]   : %s" %(tempVal)

    tempVal = iValue[6:10]
    print "SBE Previous State   : %s (%s)" %(sbeStates[tempVal], tempVal)


    tempVal = iValue[10:14]
    print "SBE Current State    : %s (%s)" %(sbeStates[tempVal], tempVal)

    tempVal = iValue[14:18]
    print "Istep Major          : %s" %(int(tempVal, 2))

    tempVal = iValue[18:26]
    print "Istep Minor          : %s" %(int(tempVal, 2))

    tempVal = iValue[26:34]
    print "Reserved Bit [24:31] : %s" %(tempVal)


def main( argv ):
    parser = argparse.ArgumentParser( description = "SBE Dump Parser" )

    parser.add_argument( '-l', '--level', choices = [ 'trace', 'attr', \
                                'ppestate', 'sbestate' , 'sbestatus'],\
                                default='trace', help = 'Parser level' )
    parser.add_argument( '-t', '--target', choices = ['AWAN', 'HW', 'FILE'], \
                                default='HW', help = 'Target type' )
    parser.add_argument( '-n', '--node', type=int , default = 0, \
                                help = 'Node Number' )
    parser.add_argument( '-p', '--proc', type=int , default = 0, \
                                help = 'Proc Number' )

    args = parser.parse_args()


    # On cronus, disabe FIFO mode
    cmdFifoOff = "setconfig USE_SBE_FIFO off"
    cmdFifoOn = "setconfig USE_SBE_FIFO on"
    rc = os.system( cmdFifoOff )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmdFifoOff, rc )
       return 1

    sbeObjDir = SBE_TOOLS_PATH;
    print "sbeObjDir", sbeObjDir
    fillSymTable(sbeObjDir, args.target)
    if ( args.level == 'all' ):
        print "Parsing everything"
        collectTrace( sbeObjDir, args.target, args.node, args.proc )
        collectAttr(  sbeObjDir, args.target, args.node, args.proc )
        sbeStatus( args.node, args.proc )
    elif ( args.level == 'trace' ):
        collectTrace( sbeObjDir, args.target, args.node, args.proc )
    elif ( args.level == 'attr' ):
        collectAttr( sbeObjDir, args.target, args.node, args.proc )
    elif ( args.level == 'ppestate' ):
        ppeState( sbeObjDir, args.target, args.node, args.proc )
    elif ( args.level == 'sbestate' ):
        sbeState( sbeObjDir, args.target, args.node, args.proc )
    elif ( args.level == 'sbestatus' ):
        sbeStatus( args.node, args.proc )

    # On cronus, Enable FIFO mode
    rc = os.system( cmdFifoOn )
    if ( rc ):
       print "ERROR running %s: %d " % ( cmdFifoOn, rc )
       return 1


if __name__ == "__main__":
    main( sys.argv )

