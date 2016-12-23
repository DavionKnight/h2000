#!/bin/bash

export PATH=/opt/ppc/eldk4.2/usr/bin:$PATH

ppc_85xxDP-gcc HHlogin.c -o HHlogin 

cp HHlogin /tftpboot/


