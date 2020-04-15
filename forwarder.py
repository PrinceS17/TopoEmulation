'''
The script is used to set iptables rules to forward VM traffic to 
remote machine like GCP.
'''

import sys, os
from multiprocessing import Process, Lock
from cmdManager import CmdManager


def print_help():
    print('Usage:   python3 forwarder.py -r N_MIN:N_MAX -a DES_ADDRESS')
    print('         [-d DPORT_BASE_HOST] [-p DES_PORT_BASE_VM] [-clear]')
    exit(0)


def main():
    # parse the parameters
    opts = {'-r':'0:5', '-d':5000, '-p':5000, '-a':'0.0.0.0'}

    do_clear = False
    cur_opt = None
    for opt in sys.argv[1:]:
        if opt == '-clear':
            do_clear = True
        elif opt in opts:
            cur_opt = opt
        elif cur_opt in opts:
            opts[cur_opt] = opt
            cur_opt = None
        else:
            print_help()

    n_min, n_max = [int(n) for n in opts['-r'].strip().split(':')]
    des_ip = opts['-a']
    dport_base = int(opts['-d'])
    des_port_base = int(opts['-p'])
    cmdor = CmdManager(dry_run=False)


    for i in range(n_min, n_max + 1):
        dport = dport_base + i
        des_port = des_port_base + i
        if not do_clear:
            cmdor.set_iptable_nat(dport, des_ip, des_port)
            print('Forward flow %s to %s:%s...' % (i, des_ip, des_port))
        else:
            cmdor.del_iptable_nat(dport, des_ip, des_port)
            print('Delete NAT fule %s to %s:%s...' % (i, des_ip, des_port))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print_help()
    
    main()

