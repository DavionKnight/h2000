/*
 * arch/powerpc/platforms/83xx/mpc830x_edd.c
 *
 * Description: MPC830x EDD board specific routines.
 * This file is based on mpc831x_rdb.c
 *
 * Copyright (C) Freescale Semiconductor, Inc. 2009. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/pci.h>
#include <linux/of_platform.h>
#include <asm/time.h>
#include <asm/ipic.h>
#include <asm/udbg.h>
#include <sysdev/fsl_pci.h>
#include <sysdev/fsl_soc.h>

#include <asm/qe.h>
#include <asm/qe_ic.h>

#include "mpc83xx.h"
#include <linux/spi/spi.h>

#define MPC8XXX_GPIO_PINS	32

#define FPGA_RESET_BIT		7
#define LED_BIT_1			9
#define LED_BIT_2			10

#define GPIO_DIR		0x00
#define GPIO_ODR		0x04
#define GPIO_DAT		0x08
#define GPIO_IER		0x0c
#define GPIO_IMR		0x10
#define GPIO_ICR		0x14

#define SICRL_ADDR_OFFSET	0x114
#if 0
static struct spi_board_info mpc830x_spi_boardinfo = {
	.bus_num = 0x7000,
	.chip_select = 0,
	.mode = SPI_MODE_3,
	.max_speed_hz = 25000000,
	.modalias = "spidev",
};
#endif
static inline u32 mpc8xxx_gpio2mask(unsigned int gpio)
{
	return 1u << (MPC8XXX_GPIO_PINS - 1 - gpio);
}

/*
 * Setup the architecture
 */
static void __init mpc830x_edd_setup_arch(void)
{
//	struct device_node *np;
//	void __iomem *immap;

#ifdef CONFIG_1588_MUX_eTSEC1
	unsigned long sicrh;
#endif

	if (ppc_md.progress)
		ppc_md.progress("mpc830x_edd_setup_arch()", 0);

#ifdef CONFIG_PCI
	for_each_compatible_node(np, "pci", "fsl,mpc8349-pci")
		mpc83xx_add_bridge(np);
	for_each_compatible_node(np, "pci", "fsl,mpc8308-pcie")
		mpc83xx_add_bridge(np);
	ppc_md.pci_exclude_device = mpc83xx_exclude_device;
#endif
	
	//mpc831x_usb_cfg();

#ifdef CONFIG_1588_MUX_eTSEC1
	immap = ioremap(get_immrbase(), 0x1000);
	if (immap) {
		sicrh = in_be32(immap + MPC83XX_SICRH_OFFS);
		sicrh &= ~MPC8308_SICRH_1588_MASK;
		sicrh |= MPC8308_SICRH_1588_PPS;
		out_be32(immap + MPC83XX_SICRH_OFFS, sicrh);
		iounmap(immap);
	} else {
		printk(KERN_INFO "1588 muxing could not be done,"
				" mapping failed\n"); }
#endif
#if 0 /* delete fpga reset  */
	np = of_find_node_by_type(NULL, "fpga");
	if(np)
	{
		immap = ioremap(get_immrbase(), 0x600);
		if(immap)
		{
			int			i;
			int			b = mpc8xxx_gpio2mask(FPGA_RESET_BIT);

			// output
			setbits32(immap + GPIO_DIR, b);

			// reset the FPGA
			clrbits32(immap + GPIO_DAT, b);
			for(i = 0; i< 200; ++i)
				;
			setbits32(immap + GPIO_DAT, b);
		}
	}
#endif	
}

static void __init mpc830x_edd_init_IRQ(void)
{
	struct device_node *np;

	np = of_find_node_by_type(NULL, "ipic");
	if (!np)
		return;

	ipic_init(np, 0);

	/* Initialize the default interrupt mapping priorities,
	 * in case the boot rom changed something on us.
	 */
	ipic_set_default_priority();
}

/*
 * Called very early, MMU is off, device-tree isn't unflattened
 */
static int __init mpc830x_edd_probe(void)
{
	unsigned long root = of_get_flat_dt_root();

	return of_flat_dt_is_compatible(root, "MPC8308EDD") ||
	       of_flat_dt_is_compatible(root, "fsl,mpc8308edd");
}

static struct of_device_id __initdata of_bus_ids[] = {
	{ .compatible = "simple-bus" },
	{ .compatible = "gianfar" },
	{},
};

static int __init declare_of_platform_devices(void)
{
	of_platform_bus_probe(NULL, of_bus_ids, NULL);
	return 0;
}
machine_device_initcall(mpc830x_edd, declare_of_platform_devices);
extern int fsl_spi_init(struct spi_board_info *board_infos,
			unsigned int num_board_infos,
			void (*activate_cs)(u8 cs, u8 polarity),
			void (*deactivate_cs)(u8 cs, u8 polarity));
static int __init mpc830x_spi_init(void)
{
	void __iomem * ADDR_SICRL;

	ADDR_SICRL = ioremap(get_immrbase() + SICRL_ADDR_OFFSET, 4);
	if (ADDR_SICRL)
	{
		// change to IO function to spi
		clrbits32(ADDR_SICRL, mpc8xxx_gpio2mask(2));
		clrbits32(ADDR_SICRL, mpc8xxx_gpio2mask(3));
	}
	else
	{
		return -1;
	}

	iounmap(ADDR_SICRL);

	//return fsl_spi_init(&mpc830x_spi_boardinfo, 1, NULL, NULL);
	return 0;
}
machine_device_initcall(mpc830x_edd, mpc830x_spi_init);

define_machine(mpc830x_edd) {
	.name			= "MPC830x EDD",
	.probe			= mpc830x_edd_probe,
	.setup_arch		= mpc830x_edd_setup_arch,
	.init_IRQ		= mpc830x_edd_init_IRQ,
	.get_irq		= ipic_get_irq,
	.restart		= mpc83xx_restart,
	.time_init		= mpc83xx_time_init,
	.calibrate_decr		= generic_calibrate_decr,
	.progress		= udbg_progress,
};


