import os, sys, time
import subprocess as sp
from multiprocessing import Process, Lock
import shlex


def print_help():
    print('Usage:   python3 iperf_server -r N_MIN:N_MAX [-p PORT_BASE]')
    exit(0)


def main():
    opts = {'-r':'0:10', '-p':5000}
    cur_opt = None
    for opt in sys.argv[1:]:
        if opt in opts:
            cur_opt = opt
        elif cur_opt in opts:
            opts[cur_opt] = opt
            cur_opt = None
        else:
            print_help()
    
    n_min, n_max = [int(n) for n in opts['-r'].strip().split(':')]
    port_base = int(opts['-p'])
    for i in range(n_min, n_max + 1):
        port = port_base + i
        cmd = 'iperf3 -s -p %s' % port
        args = shlex.split(cmd)
        sp.Popen(args)
        print(cmd)
    
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print_help()
    main()