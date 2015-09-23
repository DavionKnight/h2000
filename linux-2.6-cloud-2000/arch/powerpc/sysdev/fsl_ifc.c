/*
 * Copyright 2011 Freescale Semiconductor, Inc
 *
 * Freescale Integrated Flash Controller
 *
 * Author: Dipen Dudhat <Dipen.Dudhat@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/of.h>
#include <asm/prom.h>
#include <asm/fsl_ifc.h>

struct fsl_ifc_ctrl *fsl_ifc_ctrl_dev;
EXPORT_SYMBOL(fsl_ifc_ctrl_dev);

/*
 * convert_ifc_address - convert the base address
 * @addr_base:	base address of the memory bank
 */
unsigned int convert_ifc_address(phys_addr_t addr_base)
{
	return addr_base & CSPR_BA;
}
EXPORT_SYMBOL(convert_ifc_address);

/*
 * fsl_ifc_find - find IFC bank
 * @addr_base:	base address of the memory bank
 *
 * This function walks IFC banks comparing "Base address" field of the CSPR
 * registers with the supplied addr_base argument. When bases match this
 * function returns bank number (starting with 0), otherwise it returns
 * appropriate errno value.
 */
int fsl_ifc_find(phys_addr_t addr_base)
{
	int i = 0;

	if (!fsl_ifc_ctrl_dev || !fsl_ifc_ctrl_dev->regs)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(fsl_ifc_ctrl_dev->regs->cspr_cs); i++) {
		__be32 cspr = in_be32(&fsl_ifc_ctrl_dev->regs->cspr_cs[i].cspr);
		if (cspr & CSPR_V && (cspr & CSPR_BA) ==
				convert_ifc_address(addr_base))
			return i;
	}

	return -ENOENT;
}
EXPORT_SYMBOL(fsl_ifc_find);

static int __devinit fsl_ifc_ctrl_init(struct fsl_ifc_ctrl *ctrl)
{
	struct fsl_ifc_regs __iomem *ifc = ctrl->regs;

	/*
	 * Clear all the common status and event registers
	 */
	if (in_be32(&ifc->cm_evter_stat) & IFC_CM_EVTER_STAT_CSER)
		out_be32(&ifc->cm_evter_stat, IFC_CM_EVTER_STAT_CSER);

	/* enable all error and events */
	out_be32(&ifc->cm_evter_en, IFC_CM_EVTER_EN_CSEREN);

	/* enable all error and event interrupts */
	out_be32(&ifc->cm_evter_intr_en, IFC_CM_EVTER_INTR_EN_CSERIREN);
	out_be32(&ifc->cm_erattr0, 0x0);
	out_be32(&ifc->cm_erattr1, 0x0);

	return 0;
}

static int __devexit fsl_ifc_ctrl_remove(struct of_device *ofdev)
{
	struct fsl_ifc_ctrl *ctrl = dev_get_drvdata(&ofdev->dev);

	if (ctrl->irq)
		free_irq(ctrl->irq, ctrl);

	if (ctrl->regs)
		iounmap(ctrl->regs);

	dev_set_drvdata(&ofdev->dev, NULL);
	kfree(ctrl);

	return 0;
}
/*
 * NOTE: This interrupt is used to report ifc events of various kinds,
 * such as transaction errors on the chipselects.
 */
static irqreturn_t fsl_ifc_ctrl_irq(int irqno, void *data)
{
	struct fsl_ifc_ctrl *ctrl = data;
	struct fsl_ifc_regs __iomem *ifc = ctrl->regs;
	u32 err_axiid, err_srcid, status, cs_err, err_addr;
	u32 nand_stat;

	/* read for chip select error */
	cs_err = in_be32(&ifc->cm_evter_stat);
	if (cs_err) {
		dev_err(ctrl->dev, "transaction sent to IFC is not mapped to"
				"any memory bank 0x%08X\n", cs_err);
		/* clear the chip select error */
		out_be32(&ifc->cm_evter_stat, IFC_CM_EVTER_STAT_CSER);
	}

	/* read error attribute registers print the error information */
	status = in_be32(&ifc->cm_erattr0);
	err_addr = in_be32(&ifc->cm_erattr1);

	nand_stat = in_be32(&ifc->ifc_nand.nand_evter_stat);

	/* Clear common error attribute registers */
	out_be32(&ifc->cm_erattr0, 0x0);
	out_be32(&ifc->cm_erattr1, 0x0);

	if (nand_stat) {
		/* store the NAND error status */
		ctrl->status = nand_stat;
		/* clear nand errors */
		out_be32(&ifc->ifc_nand.nand_evter_stat, nand_stat);

		if (nand_stat & IFC_NAND_EVTER_STAT_FTOER) {
			dev_err(ctrl->dev, "NAND Flash Timeout Error: "
				"NAND_EVTER_STAT 0x%08X\n", nand_stat);
		}
		if (nand_stat & IFC_NAND_EVTER_STAT_WPER) {
			dev_err(ctrl->dev, "NAND Flash Write Protect Error: "
				"NAND_EVTER_STAT 0x%08X\n", nand_stat);
		}
		if (nand_stat & IFC_NAND_EVTER_STAT_ECCER) {
			dev_err(ctrl->dev, "NAND Uncorrectable ECC Error: "
				"NAND_EVTER_STAT 0x%08X\n", nand_stat);
		}
	}

	if (status) {
		if (status & IFC_CM_ERATTR0_ERTYP_READ)
			dev_err(ctrl->dev, "Read transaction error"
				"CM_ERATTR0 0x%08X\n", status);
		else
			dev_err(ctrl->dev, "Write transaction error"
				"CM_ERATTR0 0x%08X\n", status);

		err_axiid = (status & IFC_CM_ERATTR0_ERAID) >>
					IFC_CM_ERATTR0_ERAID_SHIFT;
		dev_err(ctrl->dev, "AXI ID of the error"
					"transaction 0x%08X\n", err_axiid);

		err_srcid = (status & IFC_CM_ERATTR0_ESRCID) >>
					IFC_CM_ERATTR0_ESRCID_SHIFT;
		dev_err(ctrl->dev, "SRC ID of the error"
					"transaction 0x%08X\n", err_srcid);

		dev_err(ctrl->dev, "Transaction Address corresponding to error"
					"ERADDR 0x%08X\n", err_addr);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

/*
 * fsl_ifc_ctrl_probe
 *
 * called by device layer when it finds a device matching
 * one our driver can handled. This code allocates all of
 * the resources needed for the controller only.  The
 * resources for the NAND banks themselves are allocated
 * in the chip probe function.
*/

static int __devinit fsl_ifc_ctrl_probe(struct of_device *ofdev,
					 const struct of_device_id *match)
{
	int ret = 0;


	dev_info(&ofdev->dev, "Freescale Integrated Flash Controller\n");

	fsl_ifc_ctrl_dev = kzalloc(sizeof(*fsl_ifc_ctrl_dev), GFP_KERNEL);
	if (!fsl_ifc_ctrl_dev)
		return -ENOMEM;

	dev_set_drvdata(&ofdev->dev, fsl_ifc_ctrl_dev);

	/* IOMAP the entire IFC region */
	fsl_ifc_ctrl_dev->regs = of_iomap(ofdev->dev.of_node, 0);
	if (!fsl_ifc_ctrl_dev->regs) {
		dev_err(&ofdev->dev, "failed to get memory region\n");
		ret = -ENODEV;
		goto err;
	}

	/* get the Controller level irq */
	fsl_ifc_ctrl_dev->irq = irq_of_parse_and_map(ofdev->dev.of_node, 0);
	if (fsl_ifc_ctrl_dev->irq == NO_IRQ) {
		dev_err(&ofdev->dev, "failed to get irq resource "
							"for IFC\n");
		ret = -ENODEV;
		goto err;
	}

	/* get the nand machine irq */
	fsl_ifc_ctrl_dev->nand_irq =
			irq_of_parse_and_map(ofdev->dev.of_node, 1);
	if (fsl_ifc_ctrl_dev->nand_irq == NO_IRQ) {
		dev_err(&ofdev->dev, "failed to get irq resource "
						"for NAND Machine\n");
		ret = -ENODEV;
		goto err;
	}

	fsl_ifc_ctrl_dev->dev = &ofdev->dev;

	ret = fsl_ifc_ctrl_init(fsl_ifc_ctrl_dev);
	if (ret < 0)
		goto err;

	ret = request_irq(fsl_ifc_ctrl_dev->irq, fsl_ifc_ctrl_irq, 0,
				"fsl-ifc", fsl_ifc_ctrl_dev);
	if (ret != 0) {
		dev_err(&ofdev->dev, "failed to install irq (%d)\n",
			fsl_ifc_ctrl_dev->irq);
		ret = fsl_ifc_ctrl_dev->irq;
		goto err;
	}

	return 0;

err:
	return ret;
}

static const struct of_device_id fsl_ifc_match[] = {
	{
		.compatible = "fsl,ifc",
	},
	{},
};

static struct of_platform_driver fsl_ifc_ctrl_driver = {
	.driver = {
		.name	= "fsl-ifc",
		.owner	= THIS_MODULE,
		.of_match_table = fsl_ifc_match,
	},
	.probe       = fsl_ifc_ctrl_probe,
	.remove      = __devexit_p(fsl_ifc_ctrl_remove),
};

static __init int fsl_ifc_init(void)
{
	int ret;

	ret = of_register_platform_driver(&fsl_ifc_ctrl_driver);
	if (ret)
		printk(KERN_ERR "fsl-ifc: Failed to register platform"
				"driver\n");

	return ret;
}

static void __exit fsl_ifc_exit(void)
{
	of_unregister_platform_driver(&fsl_ifc_ctrl_driver);
}

module_init(fsl_ifc_init);
module_exit(fsl_ifc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Freescale Semiconductor");
MODULE_DESCRIPTION("Freescale Integrated Flash Controller driver");
