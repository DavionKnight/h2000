#!/bin/bash


export KPATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 
export K_SRC_PATH=/home/kevin/works/projects/H20PN-2000/linux-2.6-cloud-2000 

export PATH=/opt/ppc/eldk4.2/usr/bin:$PATH
TARGET_ARCH=powerpc

#export CROSS_COMPILE=ppc_85xxDP-


rm ./build -rf



ppc_85xxDP-gcc bcm53101.c -o bcm53101

cp bcm53101  /tftpboot/


