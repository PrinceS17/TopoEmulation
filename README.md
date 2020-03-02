# TopoEmulation: A network emulator with VirtualBox, ns-3 and AWS

TopoEmulation is a network emulation platform with VirtualBox VM, ns-3, and AWS server. The topology is

` VM --[bridge & tap]--> ns-3 network --> local tap --[ip forwarding]--> AWS EC2 server `

It combines the usage of simulated network and actual internet links to both control bottleneck link easily and involve the real network. 

# Architecture


# How to use it
- Run forwarder and check the NAT rules in iptables:

```
$ python3 forwarder.py -r 0:5 -a $server -d 5000 -p 5000
$ sudo iptables -t nat -L
```

- Run splitter once (ns3 part will fail) to get the right side bridges, then run merger to configure VM’s NIC:
```
$ python3 splitter.py -r 0:10
$ python3 merger.py -r 0:10 -m $VM -p 5000
```

Note that larger max of the splitter range is okay if it's consistent with VM's NIC (the only consequence is that you waste some bridges).

- Ensure all parties have the scripts they want;
  - Server: iperf_server.py
  - Host: splitter, host-btnk.sh, CmdManager.py
  - VM: btnk.sh, set_iperf.py

- Run host-btnk.sh on host, then run btnk.sh in VM:
```
Host: $ ./host-btnk.sh
VM:   $ ./btnk.sh
```
And it works if the terminal in VM begins outputting some statistics dynamically.

# Reference
