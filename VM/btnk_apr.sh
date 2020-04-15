#!/bin/bash
N=12                                    # number of flow
N1=$((N-1))                             # right side of range
ts=380
port=5000
emu_path=/home/src-0
iperf_path=/home/srcuser/Developer/iperf/src
sf_path=/media/sf_folder
dbus_path=/home/srcuser/Developer/csl331/dbus/
asyn=""
export DBUS_SESSION_BUS_ADDRESS=$(dbus-launch | grep DBUS_SESSION_BUS_ADDRESS | awk -F'ADDRESS=' '{print $2}')  # for the use of ssh session
if [ $# = 1 ]; then
    asyn="-asyn"
    echo "Aync flows set up!"
fi

# monitor & control
#cd $dbus_path
#echo "python mercurius_m10.py" | at now
#echo "python vAlloc_dbus.py" | at now
#echo "python athena_v2.py 99" | at now
sleep 1
mid=$(pgrep -f mer)
aid=$(pgrep -f athena)
vid=$(pgrep -f vAlloc)
if [ -z "$mid" ]; then
    echo "Error: no mercurius running!"
elif [ -z "$aid" ]; then
    echo "Error: no athena running!"
elif [ -z "$vid" ]; then
    echo "Error: no vAlloc running!"
fi
sleep 1
echo "Mercurius & Athena started for integration test!"

# time source & iperf
cd $emu_path
echo "kvm->hpet: $(date)" >> ~/btime.out 2>&1
sudo bash -c "echo hpet > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source enabled ..."
sudo bash -c "python3 set_iperf.py -r 0:$N1 -t $ts -a 10.1.0.1 -p $port -out -s $iperf_path $asyn"
echo "hpet->   : $(date)" >> ~/btime.out 2>&1
iperf_id=$(ps aux | grep src/iperf3 | awk '{if ($11=="/bin/sh") print $2;}')
if [ -z $iperf_id ]; then
    echo "Iperf3 is stopped now."
fi
sudo bash -c "echo kvm > /sys/bus/clocksource/devices/clocksource0/current_clocksource"
echo "Hpet time source disabled ..."

# kill processes
#sh $dbus_path/kill_athena.sh            # new addition
#kill -2 $(pgrep -f athena_v2.py)
#sleep 1
#kill -9 $(pgrep -f vAlloc_dbus.py)
#sleep 1
#kill $mid
#echo "Mercurius, Athena and vAlloc killed."

# collect NPZ data
sudo ntpdate 0.us.pool.ntp.org
echo "    ->kvm: $(date)" >> ~/btime.out 2>&1
dat=/home/srcuser/Iperf_$(date)
mkdir "$dat"
cp iperf-res-*.txt "$dat"
sudo cp -r "$dat" $sf_path
echo "$dat collected."

cd $dbus_path
dat=NpzApr_$(date)
mkdir "$dat"
sleep 5                                 # wait for the saving of npz files
i=0
while [ -z "$(ls *.npz)" ]; do
    sleep 1
    echo $i
    if [ $i -gt 1 ]; then               # new addition
        echo "Wait period expired and no NPZ data collected!"
    fi
done

# collect iperf data rate
while [ "$(ls *.npz)" ]; do
   mv *.npz "$dat"
   sleep 2
done
sudo cp -r "$dat" $sf_path
echo "$dat collected."


