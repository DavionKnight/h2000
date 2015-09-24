mkfs.ubifs -r ./bin -m 2048 -e 126976  -c  6979 -o  rootfs

cp rootfs /tftpboot/p1020-ubi.fs
