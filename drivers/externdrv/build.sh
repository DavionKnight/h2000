#!/bin/sh

export ARCH=powerpc
export PATH=/home/kevin/Documents/ppc-tools/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

rm fpgatest

ppc_85xxDP-gcc sysled.c fpgardwr.c fan.c  temperature.c power.c rtc.c eeprom_api.c gpio_oper.c -o fpgatest -lm
ppc_85xxDP-gcc mem.c -o mem
cp fpgatest /tftpboot
cp mem /tftpboot

