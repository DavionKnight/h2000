
export ARCH=powerpc
export PATH=/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc spi.c spidrv.c -shared -fPIC  -o libspidrv.so

ppc_85xxDP-gcc spifpga.c -o spifpga -lspidrv -L ./
ppc_85xxDP-gcc spidpll.c -o spidpll -lspidrv -L ./
ppc_85xxDP-gcc spitest-old.c -o spitest-old 
ppc_85xxDP-gcc dpll.c -o dpll -lspidrv -L ./
ppc_85xxDP-gcc fpga.c -o fpga -lspidrv -L ./

cp spitest /tftpboot
cp spitest-old /tftpboot
cp dpll /tftpboot
cp fpga /tftpboot
cp spifpga /tftpboot
cp spidpll /tftpboot
cp libspidrv.so /tftpboot



