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
conf=brite_conf/TD_30M.conf

server=44.230.75.72                     # some ssh configurations
user=ubuntu
key_path=/home/sapphire/Dropbox/Others/remotek.pem

# 1. server
# echo "Parameter setup."
# ssh -i $key_path -T $user@$server "kill \$(pidof iperf3)"
# ssh -i $key_path -T $user@$server python3 iperf_server.py -r 0:$N1 > iperf_server.out 2>&1 &    # avoid iperf logging
# echo "ssh completed."

# # 2.0 test sudo route add
# cat ~/PASSWORD.txt | sudo -S ip route add 10.2.11.0/24 via 10.1.11.2 dev tap-left-11

# 2. local
cat ~/PASSWORD.txt | sudo -S true       # vulnerability
cd $ns3_path
NS_LOG="RateMonitor=info|time" ./waf --run "scratch/brite-emu-cross -mid=$MID -nFlow=$N \
    -tStop=$ts -changeBw=$bwOpt -infoFile=$file -confFile=$conf" & \
    (sleep 4; cd $emu_path; python3 splitter.py -r 0:$N1)

sleep 1

# 3. start the VM (dbus in mercurius doesn't work)
# VBoxManage guestcontrol SingleSrc run --exe "/home/src-0/btnk_int.sh" --username srcuser --password srcuser --wait-stdout --wait-stderr -- - $1
# sshpass -p srcuser ssh -p 2023 -T srcuser@127.0.0.1 "export DBUS_SESSION_BUS_ADDRESS=$(dbus-launch | grep DBUS_SESSION_BUS_ADDRESS | awk -F'ADDRESS=' '{print $2}'); \
    # /home/src-0/btnk_int.sh 1"