#!/bin/bash

export PATH=$PATH:/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin
PREPATH=${PWD}
rm ${PREPATH}/Output -rf
mkdir ${PREPATH}/Output

chmod +x configure
#if false;then
./configure \
	CC=ppc_85xxDP-gcc \
	--host=ppc-linux \
	--prefix=${PREPATH}/Output \
	--enable-static --disable-shared \
	--with-ksource=/home/kevin/work/H20PN-2000/linux-2.6-cloud-2000
#fi

make
make install



