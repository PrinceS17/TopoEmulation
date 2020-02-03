import os, sys, time, random
import subprocess

'''
Manager to convert VBoxManage into easy-call functions.
Note that all the default settings should be configured in the example
VM that will be used for import.
'''
class CmdManager:
    def __init__(self, dry_run=True, ovf='~/ubuntu16.04.ovf', do_print=False):
        self.dry_run = dry_run          # dry_run: no actual execution of command
        self.ovf = ovf
        self.do_print = do_print

    def exec(self, cmd=None):
        cmd = cmd if cmd else self.cmd
        if self.do_print:
            print(cmd)
        if not self.dry_run:
            os.system(cmd)
    
    def new_proc(self, args):
        # open a new process with args including the command
        if self.do_print:
            print('\n%s' % args)
        if not self.dry_run:
            proc = subprocess.Popen(args)
            return proc
        return None

    def exec_out(self, target, cmd=None):
        cmd = cmd if cmd else self.cmd
        if self.do_print:
            print(cmd)
        if self.dry_run:
            return True
        content = subprocess.getoutput(cmd)
        return content.find(target) != -1
    
    def exec_get(self, cmd=None):
        cmd = cmd if cmd else self.cmd
        if self.do_print:
            print(cmd)
        if self.dry_run:
            return None
        return subprocess.getoutput(cmd)
        
    def import_vm(self, vm, ovf=None):
        ovf = ovf if ovf else self.ovf
        self.cmd = 'VBoxManage import %s --vsys 0 --ostype Ubuntu_64 --vmname %s --cpus 1 --memory 128' \
            % (ovf, vm)
        self.exec()
    
    def set_bridge(self, vm, bridge, index=3):
        self.cmd = 'VBoxManage modifyvm %s --nic%s bridged --bridgeadapter%s %s --cableconnected%s on' \
            % (vm, index, index, bridge, index)
        self.exec()
    
    def start_vm(self, vm, gui=False):
        # self.cmd = 'VBoxManage startvm %s' % vm
        option = '' if gui else '--type headless'
        self.cmd = 'VBoxManage startvm %s %s' % (vm, option)        # no GUI start
        self.exec()
    
    def stop_vm(self, vm):
        self.cmd = 'VBoxManage controlvm %s poweroff' % vm
        self.exec()

    def copy_to_vm(self, vm, src, des, user='right', pwd='toor'):
        self.cmd = 'VBoxManage guestcontrol %s --username %s --password %s copyto --follow -R %s %s' \
            % (vm, user, pwd, src, des)
        self.exec()
    
    def copy_from_vm(self, vm, src, des, user='right', pwd='toor'):
        self.cmd = 'VBoxManage guestcontrol %s --username %s --password %s copyfrom --follow %s %s' \
            % (vm, user, pwd, src, des)
        self.exec()
    
    def chmod(self, vm, mode, file, user='right', pwd='toor'):
        self.cmd = 'VBoxManage guestcontrol %s run --exe "/bin/chmod" --username %s --password %s --wait-stdout --wait-stderr -- - %s %s' \
            % (vm, user, pwd, mode, file)
        self.exec()
    
    # important: used to run shell script and iperf (optional: if run in background)
    def run_in_vm(self, vm, sh, args, user='right', pwd='toor'):
        args_str = ''
        for opt in args:
            args_str += ' -%s %s' % (opt, args[opt])
        self.cmd = 'VBoxManage guestcontrol %s run --exe "%s" --username %s --password %s --wait-stdout --wait-stderr -- - %s' \
            % (vm, sh, user, pwd, args_str)
        self.exec()

    # general case, need first in ns3 folder
    # test needed after emulation script finished
    def run_ns3(self, program, args):
        args_str = ''
        for opt in args:
            args_str += ' -%s=%s' % (opt, args[opt])
        self.cmd = './waf --run "scratch/%s %s" ' % (program, args_str)
        self.exec()
    
    def vm_exist(self, vm):
        self.cmd = 'VBoxManage list vms | grep %s' % vm
        return self.exec_out(vm)

    def check_vm_info(self, vm, prop, target):
        # check the vm whether the property is/containes target, e.g. NIC 3 is br-left-0?, State is up?
        self.cmd = 'VBoxManage showvminfo %s | grep "%s"' % (vm, prop)
        return self.exec_out(target)
    
    def check_sh(self, vm, sh, user='right', pwd='toor'):
        self.cmd = 'VBoxManage guestcontrol %s run --exe "/bin/ls" --username %s --password %s --wait-stdout --wait-stderr -- - home/%s' \
            % (vm, user, pwd, user)
        return self.exec_out(sh)

    def check_vm_folder(self, folder):
        self.cmd = 'vboxmanage list systemproperties | grep folder'
        return self.exec_out(folder)
    
    def set_vm_folder(self, folder):
        self.cmd = 'vboxmanage setproperty machinefolder %s' % folder
        return self.exec()
    
    def check_vm_status(self, vm, user='right', pwd='toor'):
        # self.cmd = 'VBoxManage guestproperty get %s /VirtualBox/GuestInfo/Net/0/Status' % vm
        # return self.exec_out('Up')
        self.cmd = 'vboxmanage guestcontrol %s run --exe "/bin/ls" --username %s --password %s --wait-stdout --wait-stderr -- - /home/%s' \
            % (vm, user, pwd, user)
        return self.exec_out('Documents')
    
    def get_vm_pid(self, vm, program='iperf3'):
        self.cmd = 'VBoxManage guestcontrol %s run --exe "/bin/pidof" --username right --password toor \
            --wait-stdout --wait-stderr -- -  %s' % (vm, program)
        pstr = self.exec_get().strip()
        pids = pstr.split(' ')
        if len(pids) == 1:
            return int(pids[0])
        return pstr

    def get_vm_dev(self, vm):
        self.cmd = 'VBoxManage guestcontrol %s run --exe "/bin/netstat" --username right --password toor \
            --wait-stdout --wait-stderr -- - %s' % (vm, '-i')
        dstr = self.exec_get().strip()    
        dev = []
        for line in dstr.split('\n'):
            words = line.split(' ')
            for w in words:
                if w.find('enp0s') != -1 and w not in ['enp0s3', 'enp0s8']:
                    dev.append(w)
        return dev

    def get_vm_hosts(self, vm):
        self.cmd = 'VBoxManage guestcontrol %s run --exe "/bin/hostname" --username right --password toor \
            --wait-stdout --wait-stderr -- - %s' % (vm, '-I')
        istr = self.exec_get().strip()
        return istr.split(' ')

    def build_tap_bridge(self, tap, bridge):
        cmds = ['sudo true',
                'sudo brctl addbr %s' % bridge,
                'sudo tunctl -t %s' % tap,
                'sudo ifconfig %s 0.0.0.0 promisc up' % tap,
                'sudo brctl addif %s %s' % (bridge, tap),
                'sudo ifconfig %s up' % bridge]
        for cmd in cmds:
            self.exec(cmd)
    
    def destroy_tap_bridge(self, tap, bridge):
        cmds = ['sudo ifconfig %s down' % bridge,
                'sudo brctl delif %s %s' % (bridge, tap),
                'sudo brctl delbr %s' % bridge,
                'sudo ifconfig %s down' % tap,
                'sudo tunctl -d %s' % tap]
        for cmd in cmds:
            self.exec(cmd)

    def add_route(self, net, mask, gw, dev):
        self.cmd = 'sudo route add -net %s netmask %s gw %s dev %s' % (net, mask, gw, dev)
        self.exec()

    def set_iptable_nat(self, dport, des_ip, des_port):
        self.cmd = 'sudo iptables -t nat -A PREROUTING -p tcp --dport %s -j DNAT \
            --to-destination %s:%s' % (dport, des_ip, des_port)
        self.exec()

    def set_iptable_post(self):
        self.cmd = 'sudo iptables -t nat -A POSTROUTING -j MASQUERADE'
        self.exec()

    def del_iptable_nat(self, dport, des_ip, des_port):
        self.cmd = 'sudo iptables -t nat -D PREROUTING -p tcp --dport %s -j DNAT \
            --to-destination %s:%s' % (dport, des_ip, des_port)
        self.exec()
