/*
 * Author: Andy Fleming <afleming@freescale.com>
 * 	   Kumar Gala <galak@kernel.crashing.org>
 *
 * Copyright 2006-2008, 2010 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/cpu.h>

#include <asm/machdep.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mpic.h>
#include <asm/cacheflush.h>
#include <asm/dbell.h>

#include <sysdev/fsl_soc.h>

#define MPC85xx_BPTR_OFF		0x00020
#define MPC85xx_ECM_EEBPCR_OFF		0x01010
#define MPC85xx_PIC_PIR_OFF		0x41090

extern void mpc85xx_cpu_down(void) __attribute__((noreturn));
extern void __early_start(void);
extern void __secondary_start_page(void);
extern volatile unsigned long __spin_table;

struct epapr_entry {
	u32	addr_h;
	u32	addr_l;
	u32	r3_h;
	u32	r3_l;
	u32	reserved;
	u32	pir;
	u32	r6_h;
	u32	r6_l;
};

/* access per cpu vars from generic smp.c */
DECLARE_PER_CPU(int, cpu_state);

#ifdef CONFIG_HOTPLUG_CPU
static void __cpuinit
smp_85xx_mach_cpu_die(void)
{
	__get_cpu_var(cpu_state) = CPU_DEAD;
	smp_wmb();
	preempt_enable();

	local_irq_disable();
	idle_task_exit();
	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_DIS | TSR_FIS);
	mtspr(SPRN_TCR, 0);
	mpc85xx_cpu_down();
}

static void __cpuinit
smp_85xx_reset_core(int nr)
{
	__iomem u32 *ecm_vaddr;
	__iomem u32 *pic_vaddr;
	u32 pcr, pir, cpu;

	cpu = (1 << 24) << nr;
	ecm_vaddr = ioremap(get_immrbase() + MPC85xx_ECM_EEBPCR_OFF, 4);
	pcr = in_be32(ecm_vaddr);
	if (pcr & cpu) {
		pic_vaddr = ioremap(get_immrbase() + MPC85xx_PIC_PIR_OFF, 4);
		pir = in_be32(pic_vaddr);
		/* reset assert */
		pir |= (1 << nr);
		out_be32(pic_vaddr, pir);
		pir = in_be32(pic_vaddr);
		pir &= ~(1 << nr);
		/* reset negate */
		out_be32(pic_vaddr, pir);
		(void)in_be32(pic_vaddr);
		iounmap(pic_vaddr);
	} else {
		out_be32(ecm_vaddr, pcr | cpu);
		(void)in_be32(ecm_vaddr);
	}
	iounmap(ecm_vaddr);
}

static int __cpuinit
smp_85xx_map_bootpg(unsigned long pa)
{
	__iomem u32 *bootpg_ptr;
	u32 bptr;

	/* Get the BPTR */
	bootpg_ptr = ioremap(get_immrbase() + MPC85xx_BPTR_OFF, 4);

	/* Set the BPTR to the secondary boot page */
	(void)in_be32(bootpg_ptr);

	bptr = (0x80000000 | (pa >> 12));
	out_be32(bootpg_ptr, bptr);
	(void)in_be32(bootpg_ptr);
	iounmap(bootpg_ptr);
	return 0;
}

static int __cpuinit
smp_85xx_unmap_bootpg(void)
{
	__iomem u32 *bootpg_ptr;

	/* Get the BPTR */
	bootpg_ptr = ioremap(get_immrbase() + MPC85xx_BPTR_OFF, 4);

	/* Restore the BPTR */
	if (in_be32(bootpg_ptr) & 0x80000000) {
		out_be32(bootpg_ptr, 0);
		(void)in_be32(bootpg_ptr);
	}
	iounmap(bootpg_ptr);
	return 0;
}
#endif

static void __cpuinit
smp_85xx_kick_cpu(int nr)
{
	unsigned long flags;
	phys_addr_t cpu_rel_addr;
	__iomem struct epapr_entry *epapr;
	int n = 0;
	int ioremappable;

	WARN_ON (nr < 0 || nr >= NR_CPUS);

	pr_debug("smp_85xx_kick_cpu: kick CPU #%d\n", nr);

	if (system_state < SYSTEM_RUNNING) {
		/* booting, using __spin_table from u-boot */
		struct device_node *np;
		const u64 *prop;

		np = of_get_cpu_node(nr, NULL);
		if (np == NULL)
			return;

		prop = of_get_property(np, "cpu-release-addr", NULL);
		if (prop == NULL) {
			of_node_put(np);
			printk(KERN_ERR "No cpu-release-addr for cpu %d\n", nr);
			return;
		}
		cpu_rel_addr = (phys_addr_t)*prop;
		of_node_put(np);

		/*
		 * A secondary core could be in a spinloop in the bootpage
		 * (0xfffff000), somewhere in highmem, or somewhere in lowmem.
		 * The bootpage and highmem can be accessed via ioremap(), but
		 * we need to directly access the spinloop if its in lowmem.
		 */
		ioremappable = cpu_rel_addr > virt_to_phys(high_memory);

		if (ioremappable)
			epapr = ioremap(cpu_rel_addr,
						sizeof(struct epapr_entry));
		else
			epapr = phys_to_virt(cpu_rel_addr);

		local_irq_save(flags);
	} else {
#ifdef CONFIG_HOTPLUG_CPU
		/* hotplug, using __spin_table from kernel */
		cpu_rel_addr = (__pa(&__spin_table) + nr *
					sizeof(struct epapr_entry));
		pr_debug("cpu-release-addr=%llx, __spin_table=%p, nr=%x\n",
					(unsigned long long)cpu_rel_addr,
					&__spin_table, nr);

		/* prevent bootpage from being accessed by others */
		local_irq_save(flags);

		smp_85xx_map_bootpg(__pa(__secondary_start_page));

		/* remap the 0xFFFFF000 page as non-cacheable */
		ioremappable = 1;
		epapr = ioremap(cpu_rel_addr | 0xfffff000, sizeof(struct epapr_entry));

		smp_85xx_reset_core(nr);

		/* wait until core(nr) is ready... */
		while ((in_be32(&epapr->addr_l) != 1) && (++n < 1000))
			udelay(100);

		if (n == 1000) {
			pr_err("timeout waiting for core%d to reset\n",
					nr);
			return;
		}
#else
		pr_err("runtime kick cpu not supported\n");
		return;
#endif
	}

	out_be32(&epapr->pir, nr);
	out_be32(&epapr->addr_l, __pa(__early_start));

	if (!ioremappable)
		flush_dcache_range((ulong)epapr,
				(ulong)epapr + sizeof(struct epapr_entry));

	/* Wait a bit for the CPU to ack. */
	n = 0;
	while ((__secondary_hold_acknowledge != nr) && (++n < 1000))
		mdelay(1);

	local_irq_restore(flags);

	if (ioremappable)
		iounmap(epapr);

#ifdef CONFIG_HOTPLUG_CPU
	if (system_state >= SYSTEM_RUNNING)
		smp_85xx_unmap_bootpg();
#endif

	pr_debug("waited %d msecs for CPU #%d.\n", n, nr);
}

struct smp_ops_t smp_85xx_ops = {
};

void __init mpc85xx_smp_init(void)
{
	struct device_node *np;

	np = of_find_node_by_type(NULL, "open-pic");
	if (np) {
		smp_85xx_ops.probe = smp_mpic_probe;
		smp_85xx_ops.setup_cpu = smp_mpic_setup_cpu;
		smp_85xx_ops.message_pass = smp_mpic_message_pass;
		smp_85xx_ops.kick_cpu = smp_85xx_kick_cpu;
#ifdef CONFIG_HOTPLUG_CPU
		smp_85xx_ops.give_timebase = smp_generic_give_timebase;
		smp_85xx_ops.take_timebase = smp_generic_take_timebase;
		smp_85xx_ops.cpu_disable   = generic_cpu_disable;
		smp_85xx_ops.cpu_die	= generic_cpu_die;
		ppc_md.cpu_die		= smp_85xx_mach_cpu_die;
#endif
	}

	if (cpu_has_feature(CPU_FTR_DBELL))
		smp_85xx_ops.message_pass = smp_dbell_message_pass;

	BUG_ON(!smp_85xx_ops.message_pass);

	smp_ops = &smp_85xx_ops;
}
