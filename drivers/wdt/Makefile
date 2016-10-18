##############################################################################
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
# PROJECT:
#
# ABSTRACT:
#
# DEPENDENCIES:
# 
# Author: Stefan Stürke (sst@pandatel.com)
#         changed by Ingo Assmus (ias@pandatel.com)
#
##############################################################################
# $Id: Makefile,v 1.12 2005/12/19 10:38:20 sst Exp $
#
##############################################################################

ifndef TOPDIR
	TOPDIR	:= $(shell cd ../.. && pwd)
	export TOPDIR
endif
#include $(TOPDIR)/config.mk

##############################################################################
# external variables:
#
# Project name (from config.mk):
#PROJECT=
#
# Install path (form config.mk):
INSTALL_PATH=$(RESULTS)/targetfs/opt/active
#
# To force the rebuilding, define the following:
#PEDANTIC=true
#
# To force installing, define the following:
#INST_PEDANTIC=true
#
# Location to store the documentation:
DOC_PATH=$(TOPDIR)/doc/drivers/wdt
# 

BUILD_PATH=build/$(PROJECT)
SRC_PATH=src

#CLONE_TREE=$(TOPDIR)/tools/clone_tree

USER_ID:=$(shell id -u)

ifdef PEDANTIC
	PREREQ=forcebuild
endif

ifdef INST_PEDANTIC
	INST_PREREQ=forcebuild
endif

KERNEL_UTS_RELEASE:= $(shell grep UTS_RELEASE $(STD_INC_PATH)/linux/version.h | awk '{print $$3}' | sed -e 's/"//g')

all: clone wdt

.PHONY: install
install: all $(INST_PREREQ)
	install -D -m 644 $(BUILD_PATH)/wdt.ko $(INSTALL_PATH)/lib/modules/$(KERNEL_UTS_RELEASE)/user/wdt.o
	install -D -m 755 etc/init.d/wdt $(INSTALL_PATH)/etc/init.d/wdt
	install -D -p -m 644 src/include/ioctl_codes.h $(INSTALL_PATH)/usr/include/wdt/ioctl_codes.h
	install -D -p -m 644 src/include/wdt.h $(INSTALL_PATH)/usr/include/wdt/wdt.h
	install -D -p -m 644 src/include/TriggerWDT.h $(INSTALL_PATH)/usr/include/wdt/TriggerWDT.h
ifeq "$(USER_ID)" "1230"
	chown root:root $(INSTALL_PATH)/lib/modules/$(KERNEL_UTS_RELEASE)/user/wdt.o
	chown root:root $(INSTALL_PATH)/etc/init.d/wdt
endif


.PHONY: doc
#doc: all
doc: 
	@mkdir -p $(DOC_PATH)
	cp $(TOPDIR)/.project/Doxyfile Doxyfile.mk
	chmod u+w Doxyfile.mk
	cat Doxyfile.loc >> Doxyfile.mk
	echo "OUTPUT_DIRECTORY = $(DOC_PATH)" >> Doxyfile.mk
	doxygen Doxyfile.mk
	rm Doxyfile.mk

clone: $(BUILD_PATH)/Makefile

$(BUILD_PATH)/Makefile:
	rm -fr $(BUILD_PATH)
	@mkdir -p `dirname $(BUILD_PATH) `
	@echo "### Create Clonetree Dir from: $(SRC_PATH) at: $(BUILD_PATH)"
	#$(CLONE_TREE) $(SRC_PATH) $(BUILD_PATH)
	cp $(SRC_PATH) $(BUILD_PATH) -rf

.PHONY: clean
clean:
	$(MAKE) -C $(BUILD_PATH)/src clean

.PHONY: clobber
clobber:
	rm -fr $(BUILD_PATH)

.PHONY: switch
wdt: $(BUILD_PATH)/wdt.o

$(BUILD_PATH)/wdt.o: $(PREREQ)
	@echo "###Building wdt.o from BUILD_DIRECTORY = $(BUILD_PATH)"
	$(MAKE) -C $(BUILD_PATH) all CC=$(CROSS_COMPILE)gcc LN=$(CROSS_COMPILE)ld USRFLAGS="$(CONFIG_FLAGS)" LPATH=-L$(INSTALL_PATH)/usr/lib IPATH=-I$(STD_INC_PATH) KPATH=-I$(KPATH)/arch/$(TARGET_ARCH)


.PHONY: forcebuild
forcebuild:

#########################################################################
#
# History:
#   $Log: Makefile,v $
#   Revision 1.12  2005/12/19 10:38:20  sst
#   - adapted gme project ro global config flags
#
#   Revision 1.11  2005/12/16 16:10:35  sst
#   - new compiler flag: CONFIG_coptype
#
#   Revision 1.10  2005/11/18 16:27:35  sst
#   - preparations for NATIVE compiling
#
#   Revision 1.9  2005/11/08 12:23:42  sst
#   - removed dependency from comiler include path /.eldk-
#
#   Revision 1.8  2005/10/10 13:15:56  sst
#   changed config.mk handling
#
#   Revision 1.7  2005/06/08 11:55:05  sst
#   - prepared for using with etcb
#
#   Revision 1.6  2004/10/21 07:33:50  sst
#   installing headers with install -p to preserve date
#
#   Revision 1.5  2004/10/04 13:45:37  sst
#   installing header files
#
#   Revision 1.4  2004/08/18 08:35:23  sst
#   changed install for non root users
#
#   Revision 1.3  2004/08/16 08:24:25  ias
#   Cop RelII HW added
#
#   Revision 1.2  2004/04/13 08:37:37  ias
#   using libproc
#
#   Revision 1.1  2004/04/07 14:38:36  ias
#   initial switch driver
#
#
#########################################################################
