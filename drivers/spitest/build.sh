
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc spitest.c -o spitest

cp spitest /tftpboot
echo cp spitest /tftpboot


