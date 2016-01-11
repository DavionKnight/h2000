
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc pxm_drv_fpga.c pxm_drv_fpga_remote.c -o main

cp main /tftpboot


