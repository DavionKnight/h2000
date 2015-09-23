/*
 * MPC85xx DS Board Setup
 *
 * Author Xianghua Xiao (x.xiao@freescale.com)
 * Roy Zang <tie-fei.zang@freescale.com>
 * 	- Add PCI/PCI Exprees support
 * Copyright 2007-2010 Freescale Semiconductor Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>

#include <asm/system.h>
#include <asm/time.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <mm/mmu_decl.h>
#include <asm/prom.h>
#include <asm/udbg.h>
#include <asm/mpic.h>
#include <asm/i8259.h>
#include <asm/swiotlb.h>

#include <sysdev/fsl_soc.h>
#include <sysdev/fsl_pci.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(fmt, args...) printk(KERN_ERR "%s: " fmt, __func__, ## args)
#else
#define DBG(fmt, args...)
#endif

#ifdef CONFIG_PPC_I8259
static void mpc85xx_8259_cascade(unsigned int irq, struct irq_desc *desc)
{
	unsigned int cascade_irq = i8259_irq();

	if (cascade_irq != NO_IRQ) {
		generic_handle_irq(cascade_irq);
	}
	desc->chip->eoi(irq);
}
#endif	/* CONFIG_PPC_I8259 */

void __init mpc85xx_ds_pic_init(void)
{
	struct mpic *mpic;
	struct resource r;
	struct device_node *np;
#ifdef CONFIG_PPC_I8259
	struct device_node *cascade_node = NULL;
	int cascade_irq;
#endif
	unsigned long root = of_get_flat_dt_root();

	np = of_find_node_by_type(NULL, "open-pic");
	if (np == NULL) {
		printk(KERN_ERR "Could not find open-pic node\n");
		return;
	}

	if (of_address_to_resource(np, 0, &r)) {
		printk(KERN_ERR "Failed to map mpic register space\n");
		of_node_put(np);
		return;
	}

	if (of_flat_dt_is_compatible(root, "fsl,MPC8572DS-CAMP") ||
		of_flat_dt_is_compatible(root, "fsl,P2020DS-CAMP")) {
		mpic = mpic_alloc(np, r.start,
			MPIC_PRIMARY |
			MPIC_BIG_ENDIAN | MPIC_BROKEN_FRR_NIRQS,
			0, 256, " OpenPIC  ");
	} else {
		mpic = mpic_alloc(np, r.start,
			  MPIC_PRIMARY | MPIC_WANTS_RESET |
			  MPIC_BIG_ENDIAN | MPIC_BROKEN_FRR_NIRQS |
			  MPIC_SINGLE_DEST_CPU,
			0, 256, " OpenPIC  ");
	}

	BUG_ON(mpic == NULL);
	of_node_put(np);

	mpic_init(mpic);

#ifdef CONFIG_PPC_I8259
	/* Initialize the i8259 controller */
	for_each_node_by_type(np, "interrupt-controller")
	    if (of_device_is_compatible(np, "chrp,iic")) {
		cascade_node = np;
		break;
	}

	if (cascade_node == NULL) {
		printk(KERN_DEBUG "Could not find i8259 PIC\n");
		return;
	}

	cascade_irq = irq_of_parse_and_map(cascade_node, 0);
	if (cascade_irq == NO_IRQ) {
		printk(KERN_ERR "Failed to map cascade interrupt\n");
		return;
	}

	DBG("mpc85xxds: cascade mapped to irq %d\n", cascade_irq);

	i8259_init(cascade_node, 0);
	of_node_put(cascade_node);

	set_irq_chained_handler(cascade_irq, mpc85xx_8259_cascade);
#endif	/* CONFIG_PPC_I8259 */
}

#ifdef CONFIG_PCI
static int primary_phb_addr;
extern int uli_exclude_device(struct pci_controller *hose,
				u_char bus, u_char devfn);

static int mpc85xx_exclude_device(struct pci_controller *hose,
				   u_char bus, u_char devfn)
{
	struct device_node* node;
	struct resource rsrc;

	node = hose->dn;
	of_address_to_resource(node, 0, &rsrc);

	if ((rsrc.start & 0xfffff) == primary_phb_addr) {
		return uli_exclude_device(hose, bus, devfn);
	}

	return PCIBIOS_SUCCESSFUL;
}
#endif	/* CONFIG_PCI */

#ifdef CONFIG_PCI
/*Host/agent status can be read from porbmsr in the global utilities*/
static int get_mpc85xx_ds_host_agent(void)
{
	struct device_node *np;
	void __iomem *gur_regs;
	u32 host_agent = 7;

	np = of_find_compatible_node(NULL, NULL, "fsl,mpc8548-guts");
	if (np == NULL) {
		printk(KERN_ERR "Could not find global-utilities node\n");
		return host_agent;
	}

	gur_regs = of_iomap(np, 0);
	of_node_put(np);
	if (!gur_regs) {
		printk(KERN_ERR "Failed to map global-utilities register space\n");
		return host_agent;
	}
	host_agent = (in_be32(gur_regs + 4) & 0x00070000) >> 16 ;

	iounmap(gur_regs);

	return host_agent;
}

/*
 * To judge if the PCI(e) controller is host/agent mode through
 * the PORBMSR register.
 *     0: agent mode
 *     1: host mode
 */
static bool mpc85xx_ds_pci_is_host(u32 host_agent, resource_size_t res)
{
	switch (res & 0xfffff) {
	case 0xa000:    /* PCIe1 */
		return host_agent & 0x2;
		break;
	case 0x9000:    /* PCIe2 */
		return host_agent & 0x1;
		break;
	default:
		return true;
	}
}
#endif

/*
 * Setup the architecture
 */
#ifdef CONFIG_SMP
extern void __init mpc85xx_smp_init(void);
#endif
static void __init mpc85xx_ds_setup_arch(void)
{
#ifdef CONFIG_PCI
	struct device_node *np;
	struct pci_controller *hose;
	u32 host_agent;
#endif
	dma_addr_t max = 0xffffffff;

	if (ppc_md.progress)
		ppc_md.progress("mpc85xx_ds_setup_arch()", 0);

#ifdef CONFIG_PCI
	host_agent = get_mpc85xx_ds_host_agent();
	for_each_node_by_type(np, "pci") {
		if (of_device_is_compatible(np, "fsl,mpc8540-pci") ||
		    of_device_is_compatible(np, "fsl,mpc8548-pcie") ||
		    of_device_is_compatible(np, "fsl,p2020-pcie")) {
			struct resource rsrc;
			of_address_to_resource(np, 0, &rsrc);
			if (!mpc85xx_ds_pci_is_host(host_agent, rsrc.start))
				continue;
			if ((rsrc.start & 0xfffff) == primary_phb_addr)
				fsl_add_bridge(np, 1);
			else
				fsl_add_bridge(np, 0);

			hose = pci_find_hose_for_OF_device(np);
			max = min(max, hose->dma_window_base_cur +
					hose->dma_window_size);
		}
	}

	ppc_md.pci_exclude_device = mpc85xx_exclude_device;
#endif

#ifdef CONFIG_SMP
	mpc85xx_smp_init();
#endif

#ifdef CONFIG_SWIOTLB
	if (memblock_end_of_DRAM() > max) {
		ppc_swiotlb_enable = 1;
		set_pci_dma_ops(&swiotlb_dma_ops);
		ppc_md.pci_dma_dev_setup = pci_dma_dev_setup_swiotlb;
	}
#endif

	printk("MPC85xx DS board from Freescale Semiconductor\n");
}

#ifdef CONFIG_P2020DS_EVENT_IRQ
static irqreturn_t event_isr(int irq, void *dev_id)
{

	printk(KERN_INFO "MPC85xxDS: Event button been pushed.\n");
	return IRQ_HANDLED;
}

static int __init p2020ds_ngpixis_init(void)
{
	int event_irq, ret;
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "fsl,p2020ds-fpga");
	if (np) {
		event_irq = irq_of_parse_and_map(np, 0);
		ret = request_irq(event_irq, event_isr, 0, "event", NULL);
		if (ret)
			printk(KERN_ERR "Can't request board event int\n");
		of_node_put(np);
	}
	return 0;
}
machine_device_initcall(p2020_ds, p2020ds_ngpixis_init);
#endif

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init mpc8544_ds_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (of_flat_dt_is_compatible(root, "MPC8544DS")) {
#ifdef CONFIG_PCI
		primary_phb_addr = 0xb000;
#endif
		return 1;
	}

	return 0;
}

static struct of_device_id __initdata mpc85xxds_ids[] = {
	{ .type = "soc", },
	{ .compatible = "soc", },
	{ .compatible = "simple-bus", },
	{ .compatible = "gianfar", },
	{ .compatible = "fsl,mpc8572-guts", },
	{ .compatible = "fsl,rapidio-delta", },
	{},
};

static int __init mpc85xxds_publish_devices(void)
{
	return of_platform_bus_probe(NULL, mpc85xxds_ids, NULL);
}
machine_device_initcall(mpc8544_ds, mpc85xxds_publish_devices);
machine_device_initcall(mpc8572_ds, mpc85xxds_publish_devices);
machine_device_initcall(p2020_ds, mpc85xxds_publish_devices);

machine_arch_initcall(mpc8544_ds, swiotlb_setup_bus_notifier);
machine_arch_initcall(mpc8572_ds, swiotlb_setup_bus_notifier);
machine_arch_initcall(p2020_ds, swiotlb_setup_bus_notifier);

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init mpc8572_ds_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (of_flat_dt_is_compatible(root, "fsl,MPC8572DS")) {
#ifdef CONFIG_PCI
		primary_phb_addr = 0x8000;
#endif
		return 1;
	}

	return 0;
}

/*
 * Called very early, device-tree isn't unflattened
 */
static int __init p2020_ds_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	if (of_flat_dt_is_compatible(root, "fsl,P2020DS")) {
#ifdef CONFIG_PCI
		primary_phb_addr = 0x9000;
#endif
		return 1;
	}

	return 0;
}

define_machine(mpc8544_ds) {
	.name			= "MPC8544 DS",
	.probe			= mpc8544_ds_probe,
	.setup_arch		= mpc85xx_ds_setup_arch,
	.init_IRQ		= mpc85xx_ds_pic_init,
#ifdef CONFIG_PCI
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
#endif
	.get_irq		= mpic_get_irq,
	.restart		= fsl_rstcr_restart,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
};

define_machine(mpc8572_ds) {
	.name			= "MPC8572 DS",
	.probe			= mpc8572_ds_probe,
	.setup_arch		= mpc85xx_ds_setup_arch,
	.init_IRQ		= mpc85xx_ds_pic_init,
#ifdef CONFIG_PCI
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
#endif
	.get_irq		= mpic_get_irq,
	.restart		= fsl_rstcr_restart,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
};

define_machine(p2020_ds) {
	.name			= "P2020 DS",
	.probe			= p2020_ds_probe,
	.setup_arch		= mpc85xx_ds_setup_arch,
	.init_IRQ		= mpc85xx_ds_pic_init,
#ifdef CONFIG_PCI
	.pcibios_fixup_bus	= fsl_pcibios_fixup_bus,
#endif
	.get_irq		= mpic_get_irq,
	.restart		= fsl_rstcr_restart,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
};
