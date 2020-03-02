# TopoEmulation: A network emulator with VirtualBox, ns-3 and AWS

TopoEmulation is a network emulation platform with VirtualBox VM, ns-3, and AWS server. The topology is

` VM --[bridge & tap]--> ns-3 network --> local tap --[ip forwarding]--> AWS EC2 server `

It combines the usage of simulated network and actual internet links to both control bottleneck link easily and involve the real network. 

# How to Use
- Run forwarder and check the NAT rules in iptables:

```
$ python3 forwarder.py -r 0:5 -a $server -d 5000 -p 5000
$ sudo iptables -t nat -L
```

- Run splitter once (ns3 part will fail) to get the VM side bridges, then run merger to configure VM’s NIC:
```
$ python3 splitter.py -r 0:10
$ python3 merger.py -r 0:10 -m $VM -p 5000
```

Note that larger max of the splitter range is okay if it's consistent with VM's NIC (the only consequence is that you waste some bridges).

- Ensure all parties have the scripts they want;
  - Server: iperf_server.py
  - Host: splitter, host-btnk.sh, CmdManager.py
  - VM: btnk.sh, set_iperf.py

- Run host-btnk.sh on host (which will start ns-3 and iperf servers), then run btnk.sh in VM (will start iperf clients):
```
Host: $ ./host-btnk.sh
VM:   $ ./btnk.sh
```
And it works if the terminal in VM begins outputting some statistics dynamically.

# Architecture
The platform includes iperf clients in a VirtualBox VM, a ns-3 simulated network and iperf servers on a remote AWS EC2 instance. To force the traffic to go through ns-3 network before reaching EC2, we send the iperf traffic to taps on host and then forward the traffic to remote server.

**1. VirtualBox VM**

VirtualBox VM with ICH9 chipset(to have more than 8 NIC) and hpet(to enable the time source) enabled is needed. Several NICs will be created to connect to the bridges to ns-3 network, and interface IP and routes setting inside the VM are set. Interface IPs are hardcoded in `/etc/network/interfaces` and routes are set up by setting crontab to run `set-routes.sh` every time after booting.

**2. ns-3 network**

In ns-3, a brite generated dumbbell topology is used by running `scratch/brite-emu-host-vm` and the bandwidth of the center link and leaf link can be set. The network is connected to VM through bridges on host and on the other side connected to several local taps. 

The initial sending rates of flows should be set up in `flow_info_MID.txt`, where MID is the run ID. `brite-emu-host-vm` also supports varying bandwidth of leaf links, which can be set up in `bw_info.txt`. The format of `bw_info.txt`:
```
Leaf_bandwidth_in_2nd_120s_for_first_K_flows
Leaf_bandwidth_in_3rd_120s_for_first_K_flows
...
```
It means you can enforce the varying bandwidth on partial flows but note that their bandwidth will be the same. We think it doesn't hurt since it's reasonable that leaf links for our flows are similar.

**3. Host Settings and IP forwarding**

The following on the host needs to be set up: bridges between VM and ns-3 network, taps on the other side of ns-3 network, routes. The bridges are set by `merge.py`, taps and routes by `splitter.py`. And to forward the traffic from local taps to remote EC2 server, forwarding rules need to be added to the `iptables`, which is handled by `forwarder.py`. 

**4. AWS EC2 server**

On the remote side, it is as simple as ensuring the desired ports are not blocked and starting iperf3 (handled by `iperf_server.py`).
