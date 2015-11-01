
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc deal_fpga.c -o deal_fpga

cp deal_fpga /tftpboot


