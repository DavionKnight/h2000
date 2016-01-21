
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc remote_fpga.c -o remotefpga_old
ppc_85xxDP-gcc remote_fpga_new.c -o remotefpga

cp remotefpga /tftpboot


