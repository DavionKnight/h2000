#!/bin/sh

export ARCH=powerpc

#export PATH=$PATH:/opt/freescale/usr/local/gcc-4.1.78-eglibc-2.5.78-1/powerpc-e300c3-linux-gnu/bin:/opt/freescale/ltib/usr/bin/
#export CROSS_COMPILE=powerpc-e300c3-linux-gnu-

#export PATH=/home/liufeng/work/my_opt/eldk411/usr/bin:$PATH
#export CROSS_COMPILE=ppc_82xx-

export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin/:$PATH
export CROSS_COMPILE=ppc_82xx-

#make distclean

#make MPC8308EDD_NAND_config
#make hh_1604_defconfig

#make clean
#rm ./arch/powerpc/boot/p1020rdb-pc_32b.dtb
make all
make uImage
#make  p1020rdb-pc.dtb
make  mpc8308edd.dtb
#make jh1022_usb2.dtb
#make mpc8308edd.dtb

#cp ./arch/powerpc/boot/p1020rdb-pc.dtb /home/sxl/tftpboot/1604-dtb
#cp ./arch/powerpc/boot/uImage /home/sxl/tftpboot/uImage

cp ./arch/powerpc/boot/mpc8308edd.dtb ../mkubi-8308/bin/edd.dtb
cp ./arch/powerpc/boot/uImage ../mkubi-8308/bin/edd.uImage

