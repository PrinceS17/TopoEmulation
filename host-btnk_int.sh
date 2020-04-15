#!/bin/bash
N=12                                    # number of flow
# Nc=4                                    # number of cross traffic
N1=$((N-1))                             # right side of range
MID=0808                                # 0808 with 4 cross traffic
file=flow_info_$MID.txt                 # flow sending rate
bwOpt=0                                 # if change the bandwidth during the simulation
ts=380                                  # simulation time
ns3_path=/home/sapphire/Documents/ns3_BBR/ns-3.27    # ns3 path: subject to user modification
emu_path=/home/sapphire/scpt/VM-Emulation            # emu_path: include splitter.py, etc

# 2. local
cat ~/PASSWORD.txt | sudo -S true       # splitter needs network permission
cd $ns3_path
NS_LOG="RateMonitor=info|time" ./waf --run "scratch/brite-emu-cross -mid=$MID -nFlow=$N \
    -tStop=$ts -changeBw=$bwOpt -infoFile=$file" & \
    (sleep 4; cd $emu_path; python3 splitter.py -r 0:$N1)
