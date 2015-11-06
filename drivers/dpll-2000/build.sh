
export ARCH=powerpc
export PATH=/opt/eldk42/usr/bin:/opt/eldk42/bin:$PATH
export CROSS_COMPILE=ppc_85xxDP-

ppc_85xxDP-gcc dpll_new.c -o dpll
ppc_85xxDP-gcc idt285.c -o idt285

cp dpll idt285 /tftpboot


