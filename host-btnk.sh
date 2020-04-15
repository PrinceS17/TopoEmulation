#!/bin/bash
N=6                                     # number of flow
N1=$((N-1))                             # right side of range
file=flow_info_0606.txt                 # flow sending rate
bwOpt=1                                 # if change the bandwidth during the simulation
ts=500                                  # simulation time
ns3_path=~/Documents/ns3_BBR/ns-3.27    # ns3 path: subject to user modification
emu_path=~/scpt/VM-Emulation            # emu_path: include splitter.py, etc


cd $ns3_path
./waf --run "scratch/brite-emu-host-vm -mid=1010 -nFlow=$N -tStop=$ts -changeBw=$bwOpt -infoFile=$file -testFlow=0" & \
    (sleep 3; cd $emu_path; python3 splitter.py -r 0:$N1)
