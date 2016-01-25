#!/bin/sh

export ARCH=powerpc

#export PATH=$PATH:/opt/freescale/usr/local/gcc-4.1.78-eglibc-2.5.78-1/powerpc-e300c3-linux-gnu/bin:/opt/freescale/ltib/usr/bin/
#export CROSS_COMPILE=powerpc-e300c3-linux-gnu-

#export PATH=/home/liufeng/work/my_opt/eldk411/usr/bin:$PATH
#export CROSS_COMPILE=ppc_82xx-

#export PATH=/opt/eldk42/usr/bin:$PATH
export PATH=/home/kevin/Documents/ppc-tools/usr/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

#make distclean

#make MPC8308EDD_NAND_config
#make hh_1604_defconfig

#make clean
#rm ./arch/powerpc/boot/p1020rdb-pc_32b.dtb
make uImage
make  p1020rdb-pc.dtb
#make jh1022_usb2.dtb
#make mpc8308edd.dtb

#cp ./arch/powerpc/boot/p1020rdb-pc.dtb /home/sxl/tftpboot/1604-dtb
#cp ./arch/powerpc/boot/uImage /home/sxl/tftpboot/uImage

cp ./arch/powerpc/boot/p1020rdb-pc.dtb ../mkubi/bin/1604-dtb
cp ./arch/powerpc/boot/uImage ../mkubi/bin/uImage

