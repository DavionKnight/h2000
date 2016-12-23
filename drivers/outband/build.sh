#!/bin/bash
ctags -R
cscope -Rbq

export KPATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 

export PATH=/opt/ppc/eldk4.2/usr/bin:$PATH
TARGET_ARCH=powerpc

#export CROSS_COMPILE=ppc_85xxDP-


rm ./build -rf


make ARCH=powerpc CROSS_COMPILE=ppc_85xxDP- 

ppc_85xxDP-gcc netlink_state.c -o netlink_state
ppc_85xxDP-gcc getstate.c -o getstate
ppc_85xxDP-gcc eth0ioctl.c -o eth0ioctl 
ppc_85xxDP-gcc rawSocket.c -o rawSocket -lpthread

cp outband.ko /tftpboot/
cp netlink_state /tftpboot/
cp getstate /tftpboot/
cp eth0ioctl /tftpboot/
cp rawSocket /tftpboot/


