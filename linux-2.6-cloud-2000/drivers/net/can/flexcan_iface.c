/*
 * flexcan_iface.c
 *
 * Author: Bhaskar upadhaya <bhaskar.upadhaya@freescale.com>
 * Copyright 2011 Freescale Semiconductor, Inc.
 *
 * Based on code originally by Andrey Volkov <avolkov@varma-el.com>
 *
 * LICENCE:
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/can/platform/flexcan.h>

struct flexcan_interface flexcan_cb;

#ifdef CONFIG_FLEXCAN_OF
static unsigned long flexcan_get_clk_rate(struct clk *clock)
{
	return clock->rate;
}

static void flexcan_clk_put(struct clk *clk)
{
	kfree(clk);
}

static struct clk *flexcan_clk_get(struct device *dev, const char *id)
{
	struct clk *clock;
	u32 *clock_freq = NULL;
	u32 *clock_divider = NULL;
	int err;

	clock = kmalloc(sizeof(struct clk), GFP_KERNEL);
	if (!clock) {
		dev_err(dev, "Cannot allocate memory\n");
		err = -ENOMEM;
		goto failed_clock;
	}
	clock_freq = (u32 *) of_get_property(dev->of_node, "clock_freq", NULL);
	if (clock_freq == NULL) {
		dev_err(dev, "Cannot find clock_freq property\n");
		err = -EINVAL;
		kfree(clock);
		goto failed_clock;
	}

	clock_divider = (u32 *) of_get_property(dev->of_node, \
					"fsl,flexcan-clock-divider", NULL);
	if (clock_divider == NULL) {
		dev_err(dev, "Cannot find fsl,flexcan-clock-divider \
						property\n");
		err = -EINVAL;
		kfree(clock);
		goto failed_clock;
	}

	clock->rate = DIV_ROUND(*clock_freq / *clock_divider, 1000) * 1000;
	DBG(KERN_INFO "clock-frequency is  %lu in line %d in function %s \r\n",
			clock->rate, __LINE__, __func__);
	return clock;

 failed_clock:
	return ERR_PTR(err);
}

static void flexcan_of_resource_init(struct flexcan_interface *flexcan_cb)
{
	flexcan_cb->read = flexcan_read;
	flexcan_cb->write = flexcan_write;
	flexcan_cb->clk_enable = NULL;
	flexcan_cb->clk_disable = NULL;
	flexcan_cb->clk_get_rate = flexcan_get_clk_rate;
	flexcan_cb->clk_get = flexcan_clk_get;
	flexcan_cb->clk_put = flexcan_clk_put;
}

static int flexcan_of_probe(struct of_device *ofdev,
				const struct of_device_id *match)
{
	u64 addr, size;
	int err, irq;
	struct flexcan_resource flexcan_res;

	addr = of_translate_address(ofdev->dev.of_node,
		    of_get_address(ofdev->dev.of_node, 0, &size, NULL));
	flexcan_res.addr = addr;
	flexcan_res.size = size;
	flexcan_res.drv_name = ofdev->dev.driver->name;
	irq = irq_of_parse_and_map(ofdev->dev.of_node, 0);
	if (irq == NO_IRQ) {
		dev_err(&ofdev->dev, "cannot map to irq\n");
		err = -EINVAL;
		goto failed_req;
	}

	flexcan_res.irq = irq;
	flexcan_of_resource_init(&flexcan_cb);
	err = flexcan_core(&ofdev->dev, flexcan_res, &flexcan_cb);
	if (err) {
		dev_err(&ofdev->dev, "Flexcan Initialization failed with err \
				%d\n", err);
		err = -EINVAL;
		goto failed_req;
	}
	return 0;
 failed_req:
	return err;
}

static int flexcan_of_remove(struct of_device *ofdev)
{
	struct net_device *dev = dev_get_drvdata(&ofdev->dev);
	struct flexcan_priv *priv = netdev_priv(dev);
	u64 addr, size;

	unregister_flexcandev(dev);
	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(priv->base);

	addr = of_translate_address(ofdev->dev.of_node,
		    of_get_address(ofdev->dev.of_node, 0, &size, NULL));

	release_mem_region(addr, size);

	free_candev(dev);

	return 0;
}

static struct of_device_id flexcan_match[] = {
	{
	 .compatible = "fsl,flexcan-v1.0",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, flexcan_match);
static struct of_platform_driver flexcan_of_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = flexcan_match,
		   },
	.probe = flexcan_of_probe,
	.remove = flexcan_of_remove,
};

#else
void flexcan_plt_resource_init(struct flexcan_interface *flexcan_cb)
{
	flexcan_cb->read = readl;
	flexcan_cb->write = writel;
	flexcan_cb->clk_enable = clk_enable;
	flexcan_cb->clk_disable = clk_disable;
	flexcan_cb->clk_get_rate = clk_get_rate;
	flexcan_cb->clk_get = clk_get;
	flexcan_cb->clk_put = clk_put;
}

static int __devinit flexcan_plt_probe(struct platform_device *pdev)
{
	int err, irq;
	struct flexcan_resource flexcan_res;
	resource_size_t mem_size;
	struct resource *mem;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!mem || irq <= 0) {
		dev_err(&pdev->dev, "Cannot map to irq\n");
		err = -ENODEV;
		goto failed_get;
	}

	mem_size = resource_size(mem);
	flexcan_res.addr = mem->start;
	flexcan_res.size = mem_size;
	flexcan_res.drv_name = pdev->name;

	flexcan_plt_resource_init(&flexcan_cb);
	err = flexcan_core(&pdev->dev, flexcan_res, &flexcan_cb);
	if (err) {
		dev_err(&pdev->dev, "Flexcan Initialization failed with err \
				(%d) \n", err);
		err = -EINVAL;
		goto failed_req;
	}

	return 0;
 failed_req:
	return err;
}

static int __devexit flexcan_plt_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct flexcan_priv *priv = netdev_priv(dev);
	struct resource *mem;

	unregister_flexcandev(dev);
	platform_set_drvdata(pdev, NULL);
	iounmap(priv->base);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));

	clk_put(priv->clk);

	free_candev(dev);

	return 0;
}

static struct platform_driver flexcan_plt_driver = {
	.driver.name = DRV_NAME,
	.probe = flexcan_plt_probe,
	.remove = __devexit_p(flexcan_plt_remove),
};

#endif

static int __init flexcan_init(void)
{
	pr_info("%s netdevice driver\n", DRV_NAME);
#ifdef CONFIG_FLEXCAN_OF
	return of_register_platform_driver(&flexcan_of_driver);
#else
	return platform_driver_register(&flexcan_plt_driver);
#endif
}

static void __exit flexcan_exit(void)
{
#ifdef CONFIG_FLEXCAN_OF
	of_unregister_platform_driver(&flexcan_of_driver);
#else
	platform_driver_unregister(&flexcan_plt_driver);
#endif
	pr_info("%s: driver removed\n", DRV_NAME);
}

module_init(flexcan_init);
module_exit(flexcan_exit);

MODULE_AUTHOR("Bhaskar Upadhaya <bhaskar.upadhaya@freescale.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CAN port driver for flexcan based chip");
