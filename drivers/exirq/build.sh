#!/bin/bash
ctags -R

export KPATH=/home/kevin/works/projects/H20RN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20RN-2000/linux-2.6-cloud-2000 

export PATH=/opt/ppc/eldk4.2/usr/bin:$PATH
TARGET_ARCH=powerpc

#export CROSS_COMPILE=ppc_85xxDP-


rm ./build -rf


make ARCH=powerpc CROSS_COMPILE=ppc_85xxDP- 

ppc_85xxDP-gcc exirq.c -o exirq -lpthread

cp drv_exirq.ko  /tftpboot/
cp exirq /tftpboot/


