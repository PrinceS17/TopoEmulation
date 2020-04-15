#!/bin/bash
i=0
if [ $# = 0 ]; then
    num=34
else
    num=$1
fi
# echo "num is $num"
while [ $i -lt $num ]; do
    sudo ip route add 10.1.$i.0/24 via 10.2.$i.1 
    let i=i+1
done

