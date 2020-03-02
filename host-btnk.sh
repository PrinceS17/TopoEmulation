#!/bin/bash
N=6                                     # number of flow
N1=$((N-1))                             # right side of range
file=flow_info_0606.txt                 # flow sending rate
bwOpt=1                                 # if change the bandwidth during the simulation
ts=500                                  # simulation time
ns3_path=~/Documents/ns3_BBR/ns-3.27    # ns3 path: subject to user modification
emu_path=~/scpt/VM-Emulation            # emu_path: include splitter.py, etc

server=44.230.75.72                     # some ssh configurations
user=ubuntu
key_path=/home/sapphire/Dropbox/Others/remotek.pem

# 1. server
ssh -i $key_path -t $user@$server "kill \$(pidof iperf3)"
ssh -i $key_path -t $user@$server python3 iperf_server.py -r 0:$N1 &

# 2. local
cd $ns3_path
./waf --run "scratch/brite-emu-host-vm -mid=1010 -nFlow=$N -tStop=$ts -changeBw=$bwOpt -infoFile=$file -testFlow=0" & \
    (sleep 3; cd $emu_path; python3 splitter.py -r 0:$N1)

sleep 3

# 3. start the VM (assume SingleSrc is open)
# VBoxManage guestcontrol SingleSrc run --exe "/home/src-0/btnk.sh" --username srcuser --password srcuser --wait-stdout --wait-stderr
# ssh -p 2023 srcuser@127.0.0.1 /home/src-0/btnk.sh