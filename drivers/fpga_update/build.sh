
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc w25read.c -o w25read
ppc_85xxDP-gcc fpga_update.c -o fpga_update

cp w25read /tftpboot
cp fpga_update /tftpboot


