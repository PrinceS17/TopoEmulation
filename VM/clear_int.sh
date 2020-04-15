mid=$(pgrep -f mer)
aid=$(pgrep -f athe)
iid=$(pgrep -f iperf3)
if [ -z "$iid" ]; then
    echo "Iperf3 isn't running."
else
    sudo kill $iid
fi
sleep 3         # wait: the btnk_int is taking effect
if [ -z "$mid" ]; then
    echo "Mercurius isn't running."
else
    kill $mid
fi
if [ -z "$aid" ]; then
    echo "Athena isn't running."
else
    kill $aid
fi

echo "Mercurius, athena & iperf3 are clear now."

