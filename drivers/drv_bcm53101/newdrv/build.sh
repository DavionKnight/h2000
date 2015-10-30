#!/bin/bash


export KPATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 

export PATH=/opt/eldk42/usr/bin:$PATH
TARGET_ARCH=powerpc

#export CROSS_COMPILE=ppc_85xxDP-


rm ./build -rf


make ARCH=powerpc CROSS_COMPILE=ppc_85xxDP- 

ppc_85xxDP-gcc bcm53101.c -o bcm53101

cp drv_bcm53101.ko  /tftpboot/
cp bcm53101  /tftpboot/


