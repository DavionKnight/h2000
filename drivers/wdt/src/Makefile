##############################################################################
#
# File:    Makefile
# Author:  Ingo Assmus <ias@pandatel.com>
# Date:    Fr 06 Feb 2004 17:24:47 CET
# Version: $Id: Makefile,v 1.6 2005/12/19 10:38:20 sst Exp $
#
# Overview:
#
# (c) COPYRIGHT 2003 by Pandatel AG Germany
# All rights reserved.
#
# The Copyright to the computer program(s) herein is the property of
# Pandatel AG Germany.
# The program(s) may only be used and/or copied with the written permission
# from Pandatel AG or in accordance with the terms and conditions
# stipulated in the agreement contract under which the program(s) have been
# supplied.
#
##############################################################################

ifneq ($(KERNELRELEASE),) 

CC	= $(CROSS_COMPILE)gcc
LD	= $(CROSS_COMPILE)ld
AR	= $(CROSS_COMPILE)ar

STDLIBS	=

EXTRA_CFLAGS :=	-O2 -g \
			-Wall -Wstrict-prototypes \
			$(USRFLAGS) \
			-I. $(IFLAGS) \
			$(IPATH)  \
			$(KPATH)  \
			-I$(PWD)/include \
			-msoft-float
			
CFLAGS_APL :=	-O2 -g \
			-Wall -Wstrict-prototypes \
			-I. $(IFLAGS) \
			$(IPATH)  \
			-I ./include/ 

USER_MODULE_LDFLAGS = -L$(INSTALL_PATH)/usr/lib -lproc

EXTRA_ARFLAGS	:= rf

DRV_OBJS =	watchdog.o WDT_hardware.o proc.o

APPL_OBJ1 =	TriggerWDT.o

APPL_OBJ2 =	RegMeAtWDT.o

APPL_OBJ3 =	Reg_and_Remove_me.o

APPL_OBJ4 =	RegATask.o

TARGETS	= TriggerWDT RegMeAtWDT Reg_and_Remove_me RegATask

all: $(TARGETS)

clean:
	rm -f *.o *.a

clobber: clean
	rm -f $(API_OBJS) $(DRV_OBJS)

obj-m := wdt.o
wdt-objs := $(DRV_OBJS)

##############################################################################
# Application 1
##############################################################################
TriggerWDT: $(APPL_OBJ1)	

	$(CC) $^ -o $@

TriggerWDT.o: TriggerWDT.c 
	$(CC) $(CFLAGS_APL) -c $< -o $@
	

##############################################################################
# Application 2
##############################################################################
RegMeAtWDT: $(APPL_OBJ2)	

	$(CC) $^ -o $@

RegMeAtWDT.o: RegMeAtWDT.c 
	$(CC) $(CFLAGS_APL) -c $< -o $@
	

##############################################################################
# Application 3
##############################################################################
Reg_and_Remove_me: $(APPL_OBJ3)	

	$(CC) $^ -o $@

Reg_and_Remove_me.o: Reg_and_Remove_me.c 
	$(CC) $(CFLAGS_APL) -c $< -o $@
	
##############################################################################
# Application 4
##############################################################################
RegATask: $(APPL_OBJ4)	

	$(CC) $^ -o $@

RegATask.o: RegATask.c 
	$(CC) $(CFLAGS_APL) -c $< -o $@
	
	

else #ifneq ($(KERNELRELEASE),)
PWD  := $(shell pwd)
KDIR := $(K_SRC_PATH)
all:
	       $(MAKE) -C $(KDIR) M=$(PWD) modules

endif #ifneq ($(KERNELRELEASE),)

##############################################################################
#
# History:
#   $Log: Makefile,v $
#   Revision 1.6  2005/12/19 10:38:20  sst
#   - adapted gme project ro global config flags
#
#   Revision 1.5  2005/11/08 12:23:42  sst
#   - removed dependency from comiler include path /.eldk-
#
#   Revision 1.4  2005/06/08 11:55:05  sst
#   - prepared for using with etcb
#
#   Revision 1.3  2004/07/01 14:52:25  ias
#   floating point switch added
#
#   Revision 1.2  2004/04/13 08:37:37  ias
#   using libproc
#
#   Revision 1.1  2004/04/07 14:37:40  ias
#   initial switch driver
#
#
##############################################################################
