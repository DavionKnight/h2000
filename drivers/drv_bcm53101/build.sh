#!/bin/bash


export KPATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 

export PATH=/opt/eldk42/usr/bin:$PATH
TARGET_ARCH=powerpc

#export CROSS_COMPILE=ppc_85xxDP-


rm ./build -rf


make ARCH=powerpc CROSS_COMPILE=ppc_85xxDP- 

cp drv_bcm53101.ko  /tftpboot/


echo "copy dpll.ko finished."
