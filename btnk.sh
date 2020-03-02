#!/bin/bash
N=5                                                 # right side of range
ts=500
port=5000                                           # port on host
dbus_path=/home/srcuser/Developer/csl331/dbus/      # where to start monitoring & control
emu_path=/home/src-0                                # where to start iperf3
iperf_path=/home/srcuser/Developer/iperf/src/       # customized iperf path for additional option
sf_path=/media/sf_folder                            # where to collect the final data

# 3.1 VM
cd $dbus_path
python mercurius_m10.py &   # python subprocess, ability to kill
python athena_m.py 88 &     # subprocess
mid=$(ps aux | grep mer | awk '{if ($11=="python") print $2;}')
aid=$(ps aux | grep ath | awk '{if ($11=="python") print $2;}')
if [ -z "$mid" ]; then
    echo "Error: no mercurius running!"
elif [ -z "$aid" ]; then
    echo "Error: no athena running!"
fi
sleep 3
echo "Mercurius & Athena started ..."

cd $emu_path
# sudo su                     # TODO: install python3 on srcuser to avoid su
sudo bash -c "echo hpet > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source enabled ..."
sudo bash -c "python3 set_iperf.py -r 0:$N -t $ts -a 10.1.0.1 -p $port -out -s $iperf_path"
        # should join the process in python!!!
        # check if the process will end normally...

iperf_id=$(ps aux | grep src/iperf3 | awk '{if ($11=="/bin/sh") print $2;}')
if [ -z $iperf_id ]; then
    echo "Iperf3 is stopped now."
fi
sudo bash -c "echo kvm > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source disabled ..."
kill $mid
kill -INT $aid
echo "Mercurius & Athena killed."
cd $dbus_path
dat=NpzData_$(date)
mkdir "$dat"
sleep 10            # wait for the saving of npz files
while [ -z "$(ls *.npz)" ]; do
    sleep 3
done

while [ "$(ls *.npz)" ]; do
    mv *.npz "$dat"
    sleep 3
done
sudo cp -r "$dat" $sf_path
echo "$dat collected."