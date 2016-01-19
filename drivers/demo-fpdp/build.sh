
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc demo-fpga.c -o fpga
ppc_85xxDP-gcc demo-dpll.c -o dpll

cp fpga /tftpboot
cp dpll /tftpboot

#cp w25read /tftpboot
#cp fpga_update /tftpboot


