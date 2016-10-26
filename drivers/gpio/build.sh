
export ARCH=powerpc
export PATH=/opt/ppc/eldk4.2/usr/bin:/opt/ppc/eldk4.2/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc gpiodrv.c watchdog.c -shared -fPIC  -o libgpiodrv.so
ppc_85xxDP-gcc led.c -o led -lgpiodrv -L ./
ppc_85xxDP-gcc wdtdog.c -o wdtdog -lgpiodrv -L ./

cp libgpiodrv.so /tftpboot
cp led /tftpboot
cp wdtdog /tftpboot



