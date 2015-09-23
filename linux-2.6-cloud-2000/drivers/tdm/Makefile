#
# Makefile for the TDM core.
#

obj-$(CONFIG_TDM)		+= tdm-core.o
obj-$(CONFIG_TDM_CHARDEV)	+= tdm-dev.o
obj-y				+= device/ line_ctrl/ test/

ifeq ($(CONFIG_TDM_DEBUG_CORE),y)
EXTRA_CFLAGS += -DDEBUG
endif
