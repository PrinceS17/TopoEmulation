import os, sys, time
import subprocess as sp
from multiprocessing import Process, Lock
from time import sleep
import shlex


def test_ls(path=None):
    if path:
        os.chdir(path)
    tmp = os.popen('ls').read()
    tlist = tmp.strip().split('\n')
    return tlist


def print_help():
    print('Usage:   python3 set_iperf.py -r N_MIN:N_MAX [-t DURATION] [-ctrl [-c CPORT_BASE]]')
    print('                 [-a DES_ADDRESS] [-p PORT_BASE] [-s IPERF_FOLDER] [-out] [-asyn]')
    print('Note:    -slow   Enable time source.')
    exit(0)


def main():
    # assert 'inVM.sh' in test_ls()
    opts = {'-r':'0:4', '-a':'10.1.0.1', '-p':15000, '-t':10,
            '-s':'', '-c':5000}
            # '/home/srcuser/Developer/iperf/src'
    cur_opt = None
    enable_ctrl, enable_out, asyn = False, False, False
    for arg in sys.argv[1:]:
        if arg == '-ctrl':
            enable_ctrl = True
        elif arg == '-out':
            enable_out = True
        elif arg == '-asyn':
            asyn = True
        elif arg in opts:
            cur_opt = arg
        elif cur_opt in opts:
            opts[cur_opt] = arg
            cur_opt = None
        else:
            print_help()
    enable_out = False if enable_ctrl else enable_out
    
    ns = opts['-r'].split(':')
    n_min, n_max = [int(n) for n in ns]
    ip0 = opts['-a']
    port_base = int(opts['-p'])
    cport_base = int(opts['-c'])
    duration = int(opts['-t'])
    procs = []
    delay = 0
    for i in range(n_min - 1, n_max):
        if i == n_min  - 1:
            i = n_max
        elif i > n_min + 2:
            delay = 30
        port = port_base + i
        des_ip = ip0[:5] + str(i) + ip0[6:]
        iperf_path = os.path.join(opts['-s'], 'iperf3') if opts['-s'] else 'iperf3'
        cport_str = ' --cport %s' % port if enable_ctrl else ''
        out_str = ' > iperf-res-%s.txt 2>&1' % i if enable_out else ''
        log_str = ' --logfile iperf-log-%s.txt' % i if enable_ctrl else ''
        cmd = '%s -c %s -p %s -t %s%s%s%s' \
            % (iperf_path, des_ip, port, duration, cport_str, log_str, out_str)
        print("%f s: %s" % (time.time(), cmd))
        # os.popen(cmd)
        # iargs = shlex.split(cmd)
        p = sp.Popen(cmd, shell=True)
        procs.append(p)
        if asyn:
            sleep(delay)		# sleep 3s to create different enter times
    for p in procs:
        p.wait()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print_help()
    main()
