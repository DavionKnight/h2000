
export ARCH=powerpc
export PATH=/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc spifpga.c spidrv.c spi.c -o spifpga
ppc_85xxDP-gcc spidpll.c spidrv.c spi.c -o spidpll
ppc_85xxDP-gcc spitest-old.c -o spitest-old 
ppc_85xxDP-gcc dpll.c spidrv.c spi.c -o dpll
ppc_85xxDP-gcc fpga.c spidrv.c spi.c -o fpga

cp spitest /tftpboot
cp spitest-old /tftpboot
cp dpll /tftpboot
cp fpga /tftpboot
cp spifpga /tftpboot
cp spidpll /tftpboot


