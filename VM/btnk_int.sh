#!/bin/bash
N=12                                    # number of flow
N1=$((N-1))                             # right side of range
ts=90
port=5000
emu_path=/home/src-0
iperf_path=/home/srcuser/Developer/iperf/src
sf_path=/media/sf_folder
dbus_path=/home/srcuser/Developer/csl331/dbus/
export DBUS_SESSION_BUS_ADDRESS=$(dbus-launch | grep DBUS_SESSION_BUS_ADDRESS | awk -F'ADDRESS=' '{print $2}')	# for the use of ssh session

# check the argument simply
if [ $# -lt 1 ]; then
    echo "Error: mode not given!"
    exit
fi

# monitor & control
cd $dbus_path
python mercurius_m10.py &
if [ $1 = 1 ]; then
    python athena_m10.py 95 &       # case 1: 30: 7*10
elif [ $1 = 2 ]; then
    python athena_m10.py 85 &       # case 2: 50: 5*10
else
    python athena_m10_un.py 85 &    # case 3: TCP cubic
fi
mid=$(pgrep -f mer)
aid=$(pgrep -f athena)
if [ -z "$mid" ]; then
    echo "Error: no mercurius running!"
elif [ -z "$aid" ]; then
    echo "Error: no athena running!"
fi
sleep 3
echo "Mercurius & Athena started, mode: $1"

# time source & iperf
cd $emu_path
sudo bash -c "echo hpet > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source enabled ..."
sudo bash -c "python3 set_iperf.py -r 0:$N1 -t $ts -a 10.1.0.1 -p $port -out -s $iperf_path"
iperf_id=$(ps aux | grep src/iperf3 | awk '{if ($11=="/bin/sh") print $2;}')
if [ -z $iperf_id ]; then
    echo "Iperf3 is stopped now."
fi
sudo bash -c "echo kvm > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source disabled ..."

# clear & collect data of iperf data rate
kill $mid
# kill -INT $aid
kill -INT $(pgrep -f athe)
echo "Mercurius & Athena killed."
dat=/home/srcuser/Iperf_$(date)
mkdir "$dat"
cp iperf-res-*.txt "$dat"
sudo cp -r "$dat" $sf_path
sudo ntpdate 0.us.pool.ntp.org			# sync time for periodical job
kill $(pgrep -f athe)			# needed, not clear
echo "$dat collected!"

