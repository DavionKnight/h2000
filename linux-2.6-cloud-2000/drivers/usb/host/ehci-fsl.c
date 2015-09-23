/*
 * Copyright 2005-2009 MontaVista Software, Inc.
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Ported to 834x by Randy Vinson <rvinson@mvista.com> using code provided
 * by Hunter Wu.
 * Power Management support by Dave Liu <daveliu@freescale.com>,
 * Jerry Huang <Chang-Ming.Huang@freescale.com> and
 * Anton Vorontsov <avorontsov@ru.mvista.com>.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>

#include "ehci-fsl.h"

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
/* backup of ehci registers */
static int have_hcd, hcd_add;

static void do_change_hcd(struct work_struct *work)
{
	struct ehci_hcd *ehci = container_of(work, struct ehci_hcd,
			change_hcd_work);
	struct usb_hcd *hcd = ehci_to_hcd(ehci);
	void __iomem *non_ehci = hcd->regs;
	struct resource *res;
	int retval;

	res = otg_get_resources();
	if (hcd_add && !have_hcd) {
		writel(0x00000003, non_ehci + FSL_SOC_USB_USBMODE);
		retval = usb_add_hcd(hcd, res[1].start, IRQF_SHARED);
		if (retval == 0)
			have_hcd = 1;
	} else if (!hcd_add && have_hcd) {
		usb_remove_hcd(hcd);
		have_hcd = 0;
	}
}
#endif
/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */

/**
 * usb_hcd_fsl_probe - initialize FSL-based HCDs
 * @drvier: Driver to be used for this HCD
 * @pdev: USB Host Controller being probed
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller.
 *
 */
static int usb_hcd_fsl_probe(const struct hc_driver *driver,
			     struct platform_device *pdev)
{
	struct fsl_usb2_platform_data *pdata;
	struct usb_hcd *hcd;
	struct resource *res;
	int irq;
	int retval = 0;
	unsigned int temp;
#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	struct ehci_hcd *ehci;
#endif

	pr_debug("initializing FSL-SOC USB Controller\n");

	/* Need platform data for setup */
	pdata = (struct fsl_usb2_platform_data *)pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev,
			"No platform data for %s.\n", dev_name(&pdev->dev));
		return -ENODEV;
	}

	/*
	 * This is a host mode driver, verify that we're supposed to be
	 * in host mode.
	 */
	if (!((pdata->operating_mode == FSL_USB2_DR_HOST) ||
	      (pdata->operating_mode == FSL_USB2_MPH_HOST) ||
	      (pdata->operating_mode == FSL_USB2_DR_OTG))) {
		dev_err(&pdev->dev,
			"Non Host Mode configured for %s. Wrong driver linked.\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	res = otg_get_resources();
	if (!res) {
		dev_err(&pdev->dev,
				"Found HC with no IRQ!\n");
		return -ENODEV;
	}
	irq = res[1].start;
#else

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}
	irq = res->start;
#endif

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto err1;
	}

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	res = otg_get_resources();
#else
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
#endif
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no register addr. Check %s setup!\n",
			dev_name(&pdev->dev));
		retval = -ENODEV;
		goto err2;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start + 1;
#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
#else
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
				driver->description)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto err2;
	}
#endif
	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);

	if (hcd->regs == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto err3;
	}

	/* Enable USB controller */
	temp = in_be32(hcd->regs + 0x500);
	out_be32(hcd->regs + 0x500, temp | 0x4);

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	/* For OTG, default to suspend state on probe */
	printk(KERN_INFO "Suspend ehci driver for OTG auto detection\n");
	ehci = hcd_to_ehci(hcd);
	ehci->transceiver = otg_get_transceiver();
	INIT_WORK(&ehci->change_hcd_work, do_change_hcd);
	if (ehci->transceiver) {
		int status = otg_set_host(ehci->transceiver,
				&ehci_to_hcd(ehci)->self);
		dev_dbg(ehci->transceiver->dev,
				"init %s transceiver, status %d\n",
				ehci->transceiver->label, status);
		if (status) {
			if (ehci->transceiver)
				put_device(ehci->transceiver->dev);
			retval = status;
			goto err4;
		}
	} else {
		printk(KERN_INFO "can't find transceiver\n");
		retval = -ENODEV;
		goto err4;
	}
#else
	/* Set to Host mode */
	temp = in_le32(hcd->regs + 0x1a8);
	out_le32(hcd->regs + 0x1a8, temp | 0x3);

	retval = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED);
	if (retval != 0)
		goto err4;
#endif
	return retval;

      err4:
	iounmap(hcd->regs);
      err3:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
      err2:
	usb_put_hcd(hcd);
      err1:
	dev_err(&pdev->dev, "init %s fail, %d\n", dev_name(&pdev->dev), retval);
	return retval;
}

/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_fsl_remove - shutdown processing for FSL-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_fsl_probe().
 *
 */
static void usb_hcd_fsl_remove(struct usb_hcd *hcd,
			       struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

static void usb_setup_phy(struct usb_hcd *hcd,
			      enum fsl_usb2_phy_modes phy_mode,
			      unsigned int port_offset)
{
	u32 portsc = 0;
	u32 temp = 0;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	void __iomem *non_ehci = hcd->regs;

	switch (phy_mode) {
	case FSL_USB2_PHY_ULPI:
#ifdef CONFIG_PPC_85xx
		temp = in_be32(non_ehci + FSL_SOC_USB_CTRL);
		out_be32(non_ehci + FSL_SOC_USB_CTRL, temp | USB_CTRL_USB_EN |
				ULPI_PHY_CLK_SEL);
#endif
		portsc |= PORT_PTS_ULPI;
		break;
	case FSL_USB2_PHY_SERIAL:
		portsc |= PORT_PTS_SERIAL;
		break;
	case FSL_USB2_PHY_UTMI_WIDE:
		portsc |= PORT_PTS_PTW;
		/* fall through */
	case FSL_USB2_PHY_UTMI:
#ifdef CONFIG_PPC_85xx
		temp = in_be32(non_ehci + FSL_SOC_USB_CTRL);
		out_be32(non_ehci + FSL_SOC_USB_CTRL, temp | UTMI_PHY_EN |
				USB_CTRL_USB_EN);
		udelay(10*1000);  /* delay for PHY clk to appear */
#endif
		portsc |= PORT_PTS_UTMI;
		break;
	case FSL_USB2_PHY_NONE:
		break;
	}
	ehci_writel(ehci, portsc, &ehci->regs->port_status[port_offset]);
}

static void fsl_usb_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	struct fsl_usb2_platform_data *pdata;
	void __iomem *non_ehci = hcd->regs;
	u32 temp;

	pdata =
	    (struct fsl_usb2_platform_data *)hcd->self.controller->
	    platform_data;
	/* Enable PHY interface in the control reg. */
	temp = in_be32(non_ehci + FSL_SOC_USB_CTRL);
	out_be32(non_ehci + FSL_SOC_USB_CTRL, temp | 0x00000004);
	out_be32(non_ehci + FSL_SOC_USB_SNOOP1, 0x0000001b);

#if defined(CONFIG_PPC32) && !defined(CONFIG_NOT_COHERENT_CACHE)
	/*
	 * Turn on cache snooping hardware, since some PowerPC platforms
	 * wholly rely on hardware to deal with cache coherent
	 */

	/* Setup Snooping for all the 4GB space */
	/* SNOOP1 starts from 0x0, size 2G */
	out_be32(non_ehci + FSL_SOC_USB_SNOOP1, 0x0 | SNOOP_SIZE_2GB);
	/* SNOOP2 starts from 0x80000000, size 2G */
	out_be32(non_ehci + FSL_SOC_USB_SNOOP2, 0x80000000 | SNOOP_SIZE_2GB);
#endif

	if ((pdata->operating_mode == FSL_USB2_DR_HOST) ||
			(pdata->operating_mode == FSL_USB2_DR_OTG))
		usb_setup_phy(hcd, pdata->phy_mode, 0);

	if (pdata->operating_mode == FSL_USB2_MPH_HOST) {
		unsigned int chip, rev, svr;

		svr = mfspr(SPRN_SVR);
		chip = svr >> 16;
		rev = (svr >> 4) & 0xf;

		/* Deal with USB Erratum #14 on MPC834x Rev 1.0 & 1.1 chips */
		if ((rev == 1) && (chip >= 0x8050) && (chip <= 0x8055))
			ehci->has_fsl_port_bug = 1;

		if (pdata->port_enables & FSL_USB2_PORT0_ENABLED)
			usb_setup_phy(hcd, pdata->phy_mode, 0);
		if (pdata->port_enables & FSL_USB2_PORT1_ENABLED)
			usb_setup_phy(hcd, pdata->phy_mode, 1);
	}

	/* put controller in host mode. */
	ehci_writel(ehci, 0x00000003, non_ehci + FSL_SOC_USB_USBMODE);
#ifdef CONFIG_PPC_85xx
	out_be32(non_ehci + FSL_SOC_USB_PRICTRL, 0x00000008);
	out_be32(non_ehci + FSL_SOC_USB_AGECNTTHRSH, 0x00000080);
#else
	out_be32(non_ehci + FSL_SOC_USB_PRICTRL, 0x0000000c);
	out_be32(non_ehci + FSL_SOC_USB_AGECNTTHRSH, 0x00000040);
#endif
	out_be32(non_ehci + FSL_SOC_USB_SICTRL, 0x00000001);
}

/* called after powerup, by probe or system-pm "wakeup" */
int ehci_fsl_reinit(struct ehci_hcd *ehci)
{

	if (machine_is(p1021_mds)) {
#if !defined(CONFIG_FSL_USB_OTG) && !defined(CONFIG_FSL_USB_OTG_MODULE)
		fsl_usb_setup(ehci_to_hcd(ehci));
#endif
	} else {
		fsl_usb_setup(ehci_to_hcd(ehci));
	}
#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
#else
	ehci_port_power(ehci, 0);
#endif

	return 0;
}

/* called during probe() after chip reset completes */
static int ehci_fsl_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

	/* EHCI registers start at offset 0x100 */
	ehci->caps = hcd->regs + 0x100;
	ehci->regs = hcd->regs + 0x100 +
	    HC_LENGTH(ehci_readl(ehci, &ehci->caps->hc_capbase));
	dbg_hcs_params(ehci, "reset");
	dbg_hcc_params(ehci, "reset");

	/* cache this readonly data; minimize chip reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	retval = ehci_halt(ehci);
	/* It's ok that we can't halt the bus */

	/* data structure init */
	retval = ehci_init(hcd);
	if (retval)
		return retval;

	hcd->has_tt = 1;

	ehci->sbrn = 0x20;

	ehci_reset(ehci);

	retval = ehci_fsl_reinit(ehci);
	return retval;
}

struct ehci_fsl {
	struct ehci_hcd	ehci;

#ifdef CONFIG_PM
	/* Saved USB PHY settings, need to restore after deep sleep. */
	u32 usb_ctrl;
#endif
};

#ifdef CONFIG_PM

static struct ehci_fsl *hcd_to_ehci_fsl(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);

	return container_of(ehci, struct ehci_fsl, ehci);
}

static int ehci_fsl_drv_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ehci_fsl *ehci_fsl = hcd_to_ehci_fsl(hcd);
	void __iomem *non_ehci = hcd->regs;

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	if (is_otg) {
		/* remove hcd */
		hcd_add = 0;
		schedule_work(&ehci->change_hcd_work);
		hcd->state = HC_STATE_SUSPENDED;
		dev->power.power_state = PMSG_SUSPEND;
		is_otg = 0;
		return 0;
	}
#endif
	ehci_prepare_ports_for_controller_suspend(hcd_to_ehci(hcd));
	if (!fsl_deep_sleep())
		return 0;

	ehci_fsl->usb_ctrl = in_be32(non_ehci + FSL_SOC_USB_CTRL);
	return 0;
}

static int ehci_fsl_drv_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ehci_fsl *ehci_fsl = hcd_to_ehci_fsl(hcd);
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	void __iomem *non_ehci = hcd->regs;

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	if (is_otg) {
		/* add hcd */
		hcd_add = 1;
		schedule_work(&ehci->change_hcd_work);
		dev->power.power_state = PMSG_ON;
		usb_hcd_resume_root_hub(hcd);
		is_otg = 0;
		return 0;
	}
#endif
	ehci_prepare_ports_for_controller_resume(ehci);
	if (!fsl_deep_sleep())
		return 0;

	usb_root_hub_lost_power(hcd->self.root_hub);

	/* Restore USB PHY settings and enable the controller. */
	out_be32(non_ehci + FSL_SOC_USB_CTRL, ehci_fsl->usb_ctrl);

	ehci_reset(ehci);
	ehci_fsl_reinit(ehci);

	return 0;
}

static int ehci_fsl_drv_restore(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	usb_root_hub_lost_power(hcd->self.root_hub);
	return 0;
}

static struct dev_pm_ops ehci_fsl_pm_ops = {
	.suspend = ehci_fsl_drv_suspend,
	.resume = ehci_fsl_drv_resume,
	.restore = ehci_fsl_drv_restore,
};

#define EHCI_FSL_PM_OPS		(&ehci_fsl_pm_ops)
#else
#define EHCI_FSL_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct hc_driver ehci_fsl_hc_driver = {
	.description = hcd_name,
	.product_desc = "Freescale On-Chip EHCI Host Controller",
	.hcd_priv_size = sizeof(struct ehci_fsl),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset = ehci_fsl_setup,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset = ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
#ifdef CONFIG_PM
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = ehci_bus_resume,
#endif
	.relinquish_port = ehci_relinquish_port,
	.port_handed_over = ehci_port_handed_over,
	.start_port_reset = ehci_start_port_reset,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static int ehci_fsl_drv_probe(struct platform_device *pdev)
{
	if (usb_disabled())
		return -ENODEV;

	/* FIXME we only want one one probe() not two */
	return usb_hcd_fsl_probe(&ehci_fsl_hc_driver, pdev);
}

static int ehci_fsl_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

#if defined(CONFIG_FSL_USB_OTG) || defined(CONFIG_FSL_USB_OTG_MODULE)
	usb_put_hcd(hcd);
#else
	/* FIXME we only want one one remove() not two */
	usb_hcd_fsl_remove(hcd, pdev);
#endif
	return 0;
}

MODULE_ALIAS("platform:fsl-ehci");

static struct platform_driver ehci_fsl_driver = {
	.probe = ehci_fsl_drv_probe,
	.remove = ehci_fsl_drv_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "fsl-ehci",
		.pm = EHCI_FSL_PM_OPS,
	},
};
