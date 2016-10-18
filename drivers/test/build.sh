#!/bin/bash


export KPATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 

export PATH=/opt/ppc/eldk4.2/usr/bin:$PATH
TARGET_ARCH=powerpc

make ARCH=powerpc CROSS_COMPILE=ppc_85xxDP- 

cp test.ko /tftpboot/


