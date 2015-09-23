
#
# Makefile for CAAM descriptor builder library
#
# NOTE: at this point in time, this simply builds objects to link
# into the kernel as-is. This could be reconfigured as a library at
# some point in time, especially if a means to share that library
# with LWE becomes practical.
#
#
obj-$(CONFIG_CRYPTO_DEV_FSL_CAAM_DC_LIB) += caam_dcl.o

caam_dcl-objs := disasm.o cmdgen.o
