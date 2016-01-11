#!/bin/sh

export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

rm fpgatest

ppc_85xxDP-gcc sysled.c fpgardwr.c fan.c  temperature.c power.c rtc.c eeprom_api.c -o fpgatest -lm
cp fpgatest /tftpboot

