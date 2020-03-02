'''
This script is used to configure all RX bridges to connect to the same
VM, i.e. "merge" all the flows. It will configure the NIC of VM.
'''

import os, sys, time
from cmdManager import CmdManager
from multiprocessing import Process, Lock


def print_help():
    print('Usage:   python3 merger.py -r N_MIN:N_MAX [-m VM] [-p PORT_BASE_VM]')
    print('                 [-b BRIDGE_PREFIX] [-clear]')
    exit(0)


def main():
    if_base = 9
    nic_base = 3
    do_clear = False

    # parse the parameters
    opts = {'-r':'0:9', '-m':'Right\\ linux', '-p':5000, '-b':'br'}
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
    vm = opts['-m']
    port_base = opts['-p']
    br_prefix = opts['-b']

    cmdor = CmdManager(dry_run=False)
    bridges = ['%s-right-%s' % (br_prefix, i) for i in range(n_min, n_max + 1)]
    
    # check if in clear mode: doesn't work...
    if do_clear:
        pstr = cmdor.get_vm_pid(vm)
        args = {pstr:''}
        cmdor.run_in_vm(vm, '/bin/kill', args)
        print('iperf3 cleared. pid:\n%s' % pstr)
        exit(0)        

    # check if NIC is all prepared
    all_prepared = True
    for i in range(n_min, n_max + 1):
        nic = 'NIC %s' % (i - n_min + nic_base)
        if not cmdor.check_vm_info(vm, nic, bridges[i - n_min]):
            print('Not configure: %s -> %s' % (nic, bridges[i - n_min]))
            all_prepared = False
            break
    
    # configure the NIC of the VM
    if not all_prepared:
        cmdor.stop_vm(vm)
        for i in range(n_min, n_max + 1):
            cmdor.set_bridge(vm, bridges[i - n_min], nic_base + i - n_min)
        cmdor.start_vm(vm, gui=True)
    elif not cmdor.check_vm_info(vm, 'State', 'Running'):
        cmdor.start_vm(vm, gui=True)
    
    while not cmdor.check_vm_status(vm):
        time.sleep(5)


    # check inVM and set_iperf.py
    assert os.getcwd().find('VM-Emulation') != -1
    for name in ['inVM.sh', 'set_iperf.py']:
        sh_path = os.path.join(os.getcwd(), name)
        des_path = os.path.join('/home/right/', name)
        cmdor.copy_to_vm(vm, sh_path, des_path)
        if name == 'inVM.sh':
            cmdor.chmod(vm, '755', des_path)
    
    # # run inVM to configure ip and route inside
    # hpet_path = '/home/right/hpet_test'
    # local_path = os.path.join(os.getcwd(), 'hpet_test')
    # devs = cmdor.get_vm_dev(vm)
    # print('devs:')
    # for d in devs:
    #     print(d, end=' ')
    # print()
    # for i in range(N):
    #     left_ip = '10.1.%s.1' % i
    #     right_ip = '10.2.%s.2' % i
    #     gw = '10.2.%s.1' % i
    #     dev = devs[i]           # sorted assumption
    #     args = {'m':0, 'R':left_ip, 'L':right_ip, 'g':gw, 'I':dev}
    #     hosts = cmdor.get_vm_hosts(vm)
    #     args['d'] = ''
    #     cmdor.run_in_vm(vm, des_path, args)

    #     # while len(hosts) < 2 + N:
    #     #     if 'enp0s3' in hosts:
    #     #     cmdor.run_in_vm(vm, des_path, args)
    #     #     time.sleep(30)
    #     cmdor.copy_from_vm(vm, hpet_path, local_path)       # not quite necessary, just copy
    #     print('Ip and routes of flow %s configured.' % i)

    # start iperf3
    for i in range(n_min, n_max + 1):
        port = port_base + i
        args = {'m':1, 't':1, 'p':port}
        np = Process(target=cmdor.run_in_vm, args=(vm, des_path, args))
        np.start()
        print('iperf3 server of flow %s started.' % i)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print_help()
    main()