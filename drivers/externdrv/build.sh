#!/bin/sh

export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

rm fpgatest

ppc_85xxDP-gcc  fan.c sfpapi.c temperature.c sysinfo.c power.c sfpinfo.c boardinfo.c rtc.c epcs_test.c sfp_pin_info.c imageapi.c -o fpgatest -lm
cp fpgatest /tftpboot

