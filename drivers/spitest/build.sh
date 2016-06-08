
export ARCH=powerpc
export PATH=/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc spitest.c -o spitest -lpthread

cp spitest /tftpboot
echo cp spitest /tftpboot


