cscope -Rb
ctags -R
export PATH=/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin:$PATH
#export PATH=/opt/mips/eldk4.1/usr/bin:/opt/mips/eldk4.1/bin:$PATH

#mips_4KC-gcc spi.c spidrv.c fpgadrv.c dplldrv.c  --shared -fPIC  -o libspidrv.so -DH20RN181 -L ./
ppc_85xxDP-gcc spi.c spidrv.c fpgadrv.c dplldrv.c  gpiodrv.c --shared -fPIC  -o libspidrv.so -DH20RN2000 
ppc_85xxDP-gcc fpga.c -o fpga -lspidrv -L ./
ppc_85xxDP-gcc dpll.c -o dpll -lspidrv -L ./
ppc_85xxDP-gcc epcs.c -o epcs -lspidrv -L ./

cp libspidrv.so /tftpboot
cp fpga /tftpboot
cp dpll /tftpboot
cp epcs /tftpboot



