#!/bin/sh

mkfs.ubifs -r ./bin -m 2048 -e 126976 -c 300 -o kernel.img

cp kernel.img /tftpboot/
