/*
 * Copyright (C) 2007-2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Author: Geoff Thorpe, Geoff.Thorpe@freescale.com
 *
 * Description:
 * This file handles the platform specifics of the pattern-matcher driver. Ie.
 * it binds to the FPGA-over-PCI mode of operation if the PCI device is
 * detected (which has interrupt demuxing, among other differences), otherwise
 * it implements the powerpc SoC platform device handling.
 */

#include "base_private.h"

/* We support two modes of operation, FPGA-over-PCI or SoC. The former is only
 * possible if CONFIG_PCI is defined, of course. In the case both are possible,
 * the SoC mode is only enabled at run-time if a PCI device wasn't detected, we
 * use this global to determine which is running. */
int is_SoC;

/* We pull stuff out of the SoC device-tree node, here it is */
static int pme_soc;
static resource_size_t pme_soc_io;
static int pme_soc_intrs[5];

#ifdef CONFIG_PCI
struct pci_dev *cds_pci;
#define FPGAMAP_IS_ERR	0x0000e000
#define FPGAMAP_IS_DBG	0x00000060
#define FPGAMAP_IS_PM	0x0000001f
#define FPGAMAP_IS_ALL	(FPGAMAP_IS_ERR | FPGAMAP_IS_DBG | FPGAMAP_IS_PM)
static struct pme_regmap *fpgamap;
#define MY_VENDOR	0x1957
#define MY_DEVICEID	0x2801
#define MY_IS_OFFSET	(0x008 >> 2)
#endif /* CONFIG_PCI */

/****************/
/* IRQ handling */
/****************/

#ifdef CONFIG_PCI

/* The FPGA-over-PCI mode is a single PCI card with a single IRQ line and we
 * have to demux all the "interrupts" off it. Do this by having the first
 * request for an irq line initialise the IRQ handler. All others just add
 * themselves to the registration list. The irq parameter is 0-5, where 0 is
 * the common-control line and 1-5 are channels. We convert that into an
 * appropriate bitmask on the SAILFISH register that is provided to demux the
 * interrupts and whenever a real interrupt asserts, we invoke each of the
 * handlers as appropriate. */

static struct __pme_irq {
	int irq;
	u32 irq_mask;
	irq_handler_t handler;
	void *dev_id;
} pme_pci_irqs[5];
static unsigned int pme_pci_irqs_num;

static irqreturn_t pme_pci_isr(int irq, void *dev_id)
{
	static int already_warned;
	irqreturn_t ret = IRQ_NONE;
	unsigned int loop = 0;
	struct __pme_irq *p = pme_pci_irqs;
	u32 irq_bits = reg_get(fpgamap, MY_IS_OFFSET);
	if (unlikely(irq_bits & ~FPGAMAP_IS_PM)) {
		if (irq_bits & FPGAMAP_IS_ERR)
			printk(KERN_ERR PMMOD "FPGA ISR has error bits\n");
		if (irq_bits & FPGAMAP_IS_DBG)
			printk(KERN_ERR PMMOD "FPGA ISR has debug bits\n");
		if (irq_bits & ~FPGAMAP_IS_ALL)
			printk(KERN_ERR PMMOD "FPGA ISR has unknown bits\n");
		printk(KERN_ERR PMMOD "FPGA ISR = 0x%08x\n", irq_bits);
		panic("Stopping the machine, the FPGA interrupt wasn't nice");
	}
	for (; loop < pme_pci_irqs_num; loop++, p++)
		if ((p->irq_mask & irq_bits) &&
				(p->handler(p->irq, p->dev_id) == IRQ_HANDLED))
			ret = IRQ_HANDLED;
	if (unlikely(!already_warned && (ret != IRQ_HANDLED))) {
		printk(KERN_ERR PMMOD "got unexpected interrupt, "
				"irq_bits=%08x\n", irq_bits);
		already_warned = 1;
	}
	return ret;
}

#endif

int REQUEST_IRQ(enum pme_hal_irq irq, irq_handler_t handler,
		unsigned long flags, const char *devname, void *dev_id)
{
	int foo;
#ifdef CONFIG_PCI
	if (!is_SoC) {
		if (!pme_pci_irqs_num) {
			int err = request_irq(cds_pci->irq, pme_pci_isr,
					IRQF_SHARED, "CDS", cds_pci);
			if (err)
				return err;
		}
		pme_pci_irqs[pme_pci_irqs_num].irq = irq;
		pme_pci_irqs[pme_pci_irqs_num].irq_mask = 0x00000001 << irq;
		pme_pci_irqs[pme_pci_irqs_num].handler = handler;
		pme_pci_irqs[pme_pci_irqs_num++].dev_id = dev_id;
		return 0;
	}
#endif
	printk(KERN_INFO PMMOD "trying to request_irq(%d)...\n",
		pme_soc_intrs[irq]);
	foo = request_irq(pme_soc_intrs[irq], handler, flags, devname, dev_id);
	printk(KERN_INFO PMMOD "returned %d\n", foo);
	return foo;
}

void FREE_IRQ(enum pme_hal_irq irq, void *dev_id)
{
#ifdef CONFIG_PCI
	if (!is_SoC) {
		unsigned int loop = 0;
		struct __pme_irq *p = pme_pci_irqs;
		for (; loop < pme_pci_irqs_num; loop++, p++) {
			if (p->irq == irq) {
				pme_pci_irqs_num--;
				if (loop < pme_pci_irqs_num)
					memmove(p, p + 1,
						(pme_pci_irqs_num - loop) *
							sizeof(*p));
				else if (!pme_pci_irqs_num)
					free_irq(cds_pci->irq, cds_pci);
				return;
			}
		}
		BUG();
	} else
#endif
		free_irq(pme_soc_intrs[irq], dev_id);
}

/*******************/
/* Register spaces */
/*******************/

/* The get/set accessors are declared as inlines in private.h. */

static void *__map_state[] = { NULL, NULL, NULL, NULL, NULL };

int pme_regmap_init(void)
{
#ifdef CONFIG_PCI
	if (!is_SoC) {
		fpgamap = ioremap(pci_resource_start(cds_pci, 2),
				pci_resource_len(cds_pci, 2));
		if (!fpgamap)
			return -ENODEV;
		printk(KERN_INFO PMMOD "CDS FPGA version: %08x\n",
				reg_get(fpgamap, 0));
		return 0;
	}
#endif
	return 0;
}

void pme_regmap_finish(void)
{
	unsigned int loop = 0;
	while (loop < 5) {
		BUG_ON(__map_state[loop]);
		loop++;
	}
#ifdef CONFIG_PCI
	if (!is_SoC)
		iounmap(fpgamap);
#endif
}

struct pme_regmap *pme_regmap_map(unsigned int region)
{
#ifdef CONFIG_PCI
	resource_size_t io = (is_SoC ? pme_soc_io :
			pci_resource_start(cds_pci, 0));
#else
	resource_size_t io = pme_soc_io;
#endif
	io += region * 0x1000;
	__map_state[region] = ioremap(io, 0x1000);
	if (!__map_state[region]) {
		printk(KERN_ERR PMMOD "register space mapping failed\n");
		return NULL;
	}
	return __map_state[region];
}

void pme_regmap_unmap(struct pme_regmap *mapped)
{
	unsigned int loop = 0;
	while (loop < 5) {
		if (__map_state[loop] == mapped) {
			iounmap(__map_state[loop]);
			__map_state[loop] = NULL;
			return;
		}
		loop++;
	}
	BUG();
}

/*******************/
/* PCI CDS support */
/*******************/

static int pme_probe(struct platform_device *pdev)
{
	struct resource *r;
	unsigned int loop;

	if (pme_soc) {
		printk(KERN_ERR PMMOD "this driver supports one node only\n");
		return -ENODEV;
	}
	for (loop = 0; loop < 5; loop++) {
		pme_soc_intrs[loop] = platform_get_irq(pdev, loop);
		if (pme_soc_intrs[loop] < 0) {
			printk(KERN_ERR PMMOD "can't retrieve IRQ %d\n", loop);
			return -ENODEV;
		}
	}
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		printk(KERN_ERR PMMOD "can't retrieve register space\n");
		return -ENODEV;
	}
	pme_soc_io = r->start;
	pme_soc = 1;
	return 0;
}

static int pme_remove(struct platform_device *pdev)
{
	pme_soc = 0;
	return 0;
}

/* Structure for a device driver */
static struct platform_driver pme_driver = {
	.probe = pme_probe,
	.remove = pme_remove,
	.driver	= {
		.name = "fsl-pme",
	},
};

static struct device *__pme_cds_dev;
struct device *pme_cds_dev(void)
{
	return __pme_cds_dev;
}
EXPORT_SYMBOL(pme_cds_dev);

void pme_cds_dev_set(struct device *dev)
{
  __pme_cds_dev = dev;
}
EXPORT_SYMBOL(pme_cds_dev_set);

int pme_hal_init(void)
{
	int err;
#ifdef CONFIG_PCI
	cds_pci = pci_get_device(MY_VENDOR, MY_DEVICEID, NULL);
	err = (cds_pci == NULL);
	if (err)
		goto do_SoC;
	err = pci_enable_device(cds_pci);
	if (err) {
		printk(KERN_CRIT PMMOD "couldn't enable PM PCI device\n");
		pci_dev_put(cds_pci);
		return err;
	}
	is_SoC = 0;
	__pme_cds_dev = &cds_pci->dev;
	printk(KERN_INFO PMMOD "using detected PCI device\n");
	return 0;
do_SoC:
#endif
	is_SoC = 1;
	err = platform_driver_register(&pme_driver);
	if (err) {
		printk(KERN_ERR PMMOD "failure to register platform driver\n");
		return err;
	}
	if (!pme_soc) {
		printk(KERN_ERR PMMOD "no SoC node, ignoring\n");
		platform_driver_unregister(&pme_driver);
		return -ENODEV;
	}
	printk(KERN_INFO PMMOD "using SoC node\n");
	return err;
}

void pme_hal_finish(void)
{
#ifdef CONFIG_PCI
	if (!is_SoC) {
		__pme_cds_dev = NULL;
		/* Don't do this - renders it unenablable (if such a word
		 * exists). */
		/* pci_disable_device(cds_pci); */
		if (cds_pci) {
			pci_dev_put(cds_pci);
			cds_pci = NULL;
			printk(KERN_INFO PMMOD "releasing PCI device\n");
		}
		return;
	}
#endif
	/* SoC */
	platform_driver_unregister(&pme_driver);
	printk(KERN_INFO PMMOD "releasing SoC node driver\n");
}
