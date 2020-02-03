'''
The script is used to set up 
    1) NAT forwarding rules from host tap to VM;
    2) route on host to VM;
    3) tap & bridges on host.

'''
import sys, os
from multiprocessing import Process, Lock
from cmdManager import CmdManager


def print_help():
    print('Usage:   python3 splitter.py -r N_MIN:N_MAX [-d DPORT_BASE_HOST] [-p DES_PORT_BASE_VM]')
    print('         [-t TAP_PREFIX] [-b BRIDGE_PREFIX] [-m MASK] [-clear] ')
    print('         -clear    clear mode: clear all iptables rules and tap bridges.')
    print('Note that route can only be set when destination network is reachable.')
    exit(0)


def main():
    # parse the parameters
    do_clear = False
    opts = {'-r':'0:9', '-t':'tap', '-b':'br', '-d':5000,
            '-p':5000, '-m':'255.255.255.0'}

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
    tap_prefix = opts['-t']
    br_prefix =opts['-b']
    dport_base = int(opts['-d'])
    des_port_base = int(opts['-p'])
    mask = opts['-m']
    cmdor = CmdManager(dry_run=False)

    cmdor.set_iptable_post()
    for i in range(n_min, n_max + 1):
        dport = dport_base + i
        des_port = des_port_base + i
        des_ip = '10.2.%s.2' % i
        des_net = '10.2.%s.0' % i
        gw = '10.1.%s.2' % i
        dev = '%s-left-%s' % (tap_prefix, i)        
        tap = '%s-right-%s' % (tap_prefix, i)
        bridge = '%s-right-%s' % (br_prefix, i)

        if not do_clear:
            # cmdor.set_iptable_nat(dport, des_ip, des_port)
            cmdor.add_route(des_net, mask, gw, dev)
            cmdor.build_tap_bridge(tap, bridge)
            print('Done with flow %s.' % i)
        else:
            # cmdor.del_iptable_nat(dport, des_ip, des_port)
            cmdor.destroy_tap_bridge(tap, bridge)
            print('Cleared flow %s.' % i)


if __name__ == "__main__":        
    if len(sys.argv) < 2:
        print_help()
    main()