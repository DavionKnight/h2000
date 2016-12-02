/*
 * Freescale PowerQUICC Ethernet Driver -- MIIM bus implementation
 * Provides Bus interface for MIIM regs
 *
 * Author: Andy Fleming <afleming@freescale.com>
 * Modifier: Sandeep Gopalpet <sandeep.kumar@freescale.com>
 *
 * Copyright 2002-2004, 2008-2009 Freescale Semiconductor, Inc.
 *
 * Based on gianfar_mii.c and ucc_geth_mii.c (Li Yang, Kim Phillips)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/of_platform.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/ucc.h>

#include "gianfar.h"
#include "fsl_pq_mdio.h"

/*add by zhangjj*/
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>

struct mii_bus *new_bus;
/*end*/
struct fsl_pq_mdio_priv {
	void __iomem *map;
	struct fsl_pq_mdio __iomem *regs;
};


/*
 * Write value to the PHY at mii_id at register regnum,
 * on the bus attached to the local interface, which may be different from the
 * generic mdio bus (tied to a single interface), waiting until the write is
 * done before returning. This is helpful in programming interfaces like
 * the TBI which control interfaces like onchip SERDES and are always tied to
 * the local mdio pins, which may not be the same as system mdio bus, used for
 * controlling the external PHYs, for example.
 */
int fsl_pq_local_mdio_write(struct fsl_pq_mdio __iomem *regs, int mii_id,
		int regnum, u16 value)
{
/*
	printk("reg=%lx\n", regs);

	printk("event=%x\n",in_be32(&regs->ieventm)); 
	printk("maskm=%x\n",in_be32(&regs->imaskm)); 
	printk("emapm=%x\n",in_be32(&regs->emapm)); 
	printk("miimcfg=%x\n",in_be32(&regs->miimcfg)); 
	printk("miicom=%x\n",in_be32(&regs->miimcom)); 
	printk("miiadd=%x\n",in_be32(&regs->miimadd)); 
	printk("miicon=%x\n",in_be32(&regs->miimcon)); 
	printk("miimstat=%x\n",in_be32(&regs->miimstat)); 
*/
	/* Set the PHY address and the register address we want to write */
	out_be32(&regs->miimadd, (mii_id << 8) | regnum);

	/* Write out the value we want */
	out_be32(&regs->miimcon, value);

	/* Wait for the transaction to finish */
	while (in_be32(&regs->miimind) & MIIMIND_BUSY)
		cpu_relax();

	return 0;
}
EXPORT_SYMBOL(fsl_pq_local_mdio_write);
/*
 * Read the bus for PHY at addr mii_id, register regnum, and
 * return the value.  Clears miimcom first.  All PHY operation
 * done on the bus attached to the local interface,
 * which may be different from the generic mdio bus
 * This is helpful in programming interfaces like
 * the TBI which, in turn, control interfaces like onchip SERDES
 * and are always tied to the local mdio pins, which may not be the
 * same as system mdio bus, used for controlling the external PHYs, for eg.
 */
int fsl_pq_local_mdio_read(struct fsl_pq_mdio __iomem *regs,
		int mii_id, int regnum)
{
	u16 value;

	//printk("read reg=%lx\n", regs);
	/* Set the PHY address and the register address we want to read */
	out_be32(&regs->miimadd, (mii_id << 8) | regnum);

	/* Clear miimcom, and then initiate a read */
	out_be32(&regs->miimcom, 0);
	out_be32(&regs->miimcom, MII_READ_COMMAND);

	/* Wait for the transaction to finish */
	while (in_be32(&regs->miimind) & (MIIMIND_NOTVALID | MIIMIND_BUSY))
		cpu_relax();

	/* Grab the value of the register from miimstat */
	value = in_be32(&regs->miimstat);

	//printk("read reg=%lx  mii_id =%d regnum=%x val=%x\n", regs,mii_id, regnum, value);
	return value;
}
EXPORT_SYMBOL(fsl_pq_local_mdio_read);

static struct fsl_pq_mdio __iomem *fsl_pq_mdio_get_regs(struct mii_bus *bus)
{
	struct fsl_pq_mdio_priv *priv = bus->priv;

	return priv->regs;
}

/*
 * Write value to the PHY at mii_id at register regnum,
 * on the bus, waiting until the write is done before returning.
 */
int fsl_pq_mdio_write(struct mii_bus *bus, int mii_id, int regnum, u16 value)
{
	struct fsl_pq_mdio __iomem *regs = fsl_pq_mdio_get_regs(bus);

	/* Write to the local MII regs */
	return(fsl_pq_local_mdio_write(regs, mii_id, regnum, value));
}

/*
 * Read the bus for PHY at addr mii_id, register regnum, and
 * return the value.  Clears miimcom first.
 */
struct fsl_pq_mdio __iomem *preg = NULL;
int bcm53101_get_preg(struct fsl_pq_mdio **upreg)
{
	if(preg == NULL)
		return -1;
	*upreg = preg;
	return 0;
}
EXPORT_SYMBOL(bcm53101_get_preg);

int fsl_pq_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct fsl_pq_mdio __iomem *regs = fsl_pq_mdio_get_regs(bus);

	/* Read the local MII regs */
	return(fsl_pq_local_mdio_read(regs, mii_id, regnum));
}

/* Reset the MIIM registers, and wait for the bus to free */
static int fsl_pq_mdio_reset(struct mii_bus *bus)
{
	struct fsl_pq_mdio __iomem *regs = fsl_pq_mdio_get_regs(bus);
	int timeout = PHY_INIT_TIMEOUT;

	mutex_lock(&bus->mdio_lock);

	/* Reset the management interface */
	out_be32(&regs->miimcfg, MIIMCFG_RESET);

	/* Setup the MII Mgmt clock speed */
	out_be32(&regs->miimcfg, MIIMCFG_INIT_VALUE);

	/* Wait until the bus is free */
	while ((in_be32(&regs->miimind) & MIIMIND_BUSY) && timeout--)
		cpu_relax();

	mutex_unlock(&bus->mdio_lock);

	if (timeout < 0) {
		printk(KERN_ERR "%s: The MII Bus is stuck!\n",
				bus->name);
		return -EBUSY;
	}

	return 0;
}

void fsl_pq_mdio_bus_name(char *name, struct device_node *np)
{
	const u32 *addr;
	u64 taddr = OF_BAD_ADDR;

	addr = of_get_address(np, 0, NULL, NULL);
	if (addr)
		taddr = of_translate_address(np, addr);

	snprintf(name, MII_BUS_ID_SIZE, "%s@%llx", np->name,
		(unsigned long long)taddr);
}
EXPORT_SYMBOL_GPL(fsl_pq_mdio_bus_name);

/* Scan the bus in reverse, looking for an empty spot */
static int fsl_pq_mdio_find_free(struct mii_bus *new_bus)
{
	int i;

	for (i = PHY_MAX_ADDR; i > 0; i--) {
		u32 phy_id;

		if (get_phy_id(new_bus, i, &phy_id))
		{
			return -1;
		}
		if(!strcasecmp(new_bus->id,"mdio@ffe26000"))
		{
			if(2==i)
			{
				phy_id = 30;
				break;
			}
		}
		if (phy_id == 0xffffffff)
			break;
	}

	return i;
}


#if defined(CONFIG_GIANFAR) || defined(CONFIG_GIANFAR_MODULE)
static u32 __iomem *get_gfar_tbipa(struct fsl_pq_mdio __iomem *regs, struct device_node *np)
{
	struct gfar __iomem *enet_regs;

	/*
	 * This is mildly evil, but so is our hardware for doing this.
	 * Also, we have to cast back to struct gfar because of
	 * definition weirdness done in gianfar.h.
	 */
	if(of_device_is_compatible(np, "fsl,gianfar-mdio") ||
		of_device_is_compatible(np, "fsl,gianfar-tbi") ||
		of_device_is_compatible(np, "gianfar")) {
		enet_regs = (struct gfar __iomem *)regs;
		return &enet_regs->tbipa;
	} else if (of_device_is_compatible(np, "fsl,etsec2-mdio") ||
			of_device_is_compatible(np, "fsl,etsec2-tbi")) {
		return of_iomap(np, 1);
	} else
		return NULL;
}
#endif


#if defined(CONFIG_UCC_GETH) || defined(CONFIG_UCC_GETH_MODULE)
static int get_ucc_id_for_range(u64 start, u64 end, u32 *ucc_id)
{
	struct device_node *np = NULL;
	int err = 0;

	for_each_compatible_node(np, NULL, "ucc_geth") {
		struct resource tempres;

		err = of_address_to_resource(np, 0, &tempres);
		if (err)
			continue;

		/* if our mdio regs fall within this UCC regs range */
		if ((start >= tempres.start) && (end <= tempres.end)) {
			/* Find the id of the UCC */
			const u32 *id;

			id = of_get_property(np, "cell-index", NULL);
			if (!id) {
				id = of_get_property(np, "device-id", NULL);
				if (!id)
					continue;
			}

			*ucc_id = *id;

			return 0;
		}
	}

	if (err)
		return err;
	else
		return -EINVAL;
}
#endif
#define BCM53101_C
#ifdef BCM53101_C
#define PSEPHY_ACCESS_CTRL      16
#define PSEPHY_RDWR_CTRL        17
#define PSEPHY_ACCESS_REG1      24
#define PSEPHY_ACCESS_REG2      25
#define PSEPHY_ACCESS_REG3      26
#define PSEPHY_ACCESS_REG4      27

#define PSEDOPHY                30
#define PHY0                    0
#define PHY1                    1
#define PHY2                    2
#define PHY3                    3
#define PHY4                    4

#define ACCESS_EN               1

#define OPER_RD 0x2
#define OPER_WR 0x1


int bcm_fsl_mdio_read(int mii_id, unsigned short addr, unsigned short *val)
{
        int ret = 0;

        if(NULL == preg)
        {
                printk("preg is NULL\n");
                return -1;
        }

        ret = fsl_pq_local_mdio_read(preg, mii_id, addr);
        *val = ret & 0xffff;

        return 0;

}

int bcm_fsl_mdio_write(int mii_id, unsigned short addr, unsigned short val)
{
        int ret = 0;

        if(NULL == preg)
        {
                printk("preg is NULL\n");
                return -1;
        }

        ret = fsl_pq_local_mdio_write(preg, mii_id, addr, val);

        return ret;
}

int bcm53101_c_write(unsigned char page, unsigned char addr, unsigned short *value)
{
        unsigned short mdio_val = 0;
        unsigned short ret_val = 0, s_count = 0xffff;
        int mii_id = 0;

        if(((page >= 0x10) && (page <= 0x14))||(page == 0x17))//get real port phy addr
        {
                mii_id = page - 0x10;
//              printk("mii_id=%d\n",mii_id);
                bcm_fsl_mdio_write(mii_id, addr/2, value[0]);
        }
        else                            //get psedophy addr
        {
                mii_id = PSEDOPHY;
//              printk("mii_id=%d\n",mii_id);
                page &= 0xff;
                mdio_val |= (page << 8);
                mdio_val |= ACCESS_EN;

                bcm_fsl_mdio_write(mii_id, PSEPHY_ACCESS_CTRL,mdio_val);

                bcm_fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG1,value[0]);
                bcm_fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG2,value[1]);
                bcm_fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG3,value[2]);
                bcm_fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG4,value[3]);

                mdio_val = 0;
                mdio_val |= addr << 8;
                mdio_val |= OPER_WR;
                bcm_fsl_mdio_write(mii_id, PSEPHY_RDWR_CTRL,mdio_val);

//      msleep(1);      
                do
                {
                        bcm_fsl_mdio_read(mii_id, PSEPHY_RDWR_CTRL, &mdio_val);
                        if(!(mdio_val&0x11))
                                break;
                        s_count --;
        //              msleep(1);
                }while(s_count > 0);
        }
        return 0;
}


int BCM53101_C_init()
{
	unsigned short mdio_val[4] = {0};

        gpio_direction_output(5, 0);
	msleep(10);
        gpio_direction_output(5, 1);

        gpio_direction_output(8, 1);
        gpio_direction_output(13, 1);

        //set port led in a correct status
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x2012;
        bcm53101_c_write(0, 0x12, mdio_val);

        //set IMP port enable 
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x80;
        bcm53101_c_write(2, 0x0, mdio_val);

        //set IMP port receive uni/multi/broad cast enable 
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x1c;
        bcm53101_c_write(0, 0x08, mdio_val);

        //disable broad header for IMP port
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x0;
        bcm53101_c_write(2, 0x03, mdio_val);

        //set Switch Mode forwarding enable and managed mode
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x3;
        bcm53101_c_write(0, 0x0b, mdio_val);

        //set IMP Port State 1000M duplex and Link pass
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x8b;
        bcm53101_c_write(0, 0x0e, mdio_val);

        //set IMP RGMII RX/TX clock delay enable
        memset(mdio_val, 0, sizeof(mdio_val));
        mdio_val[0] = 0x3;
        bcm53101_c_write(0, 0x60, mdio_val);

	return 0;
}
#endif
static int fsl_pq_mdio_probe(struct of_device *ofdev,
		const struct of_device_id *match)
{
	struct device_node *np = ofdev->dev.of_node;
	struct device_node *tbi;
	struct fsl_pq_mdio_priv *priv;
	struct fsl_pq_mdio __iomem *regs = NULL;
	void __iomem *map;
	u32 __iomem *tbipa;
	int tbiaddr = -1;
	const u32 *addrp;
	u64 addr = 0, size = 0;
	int err;
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	new_bus = mdiobus_alloc();
	if (!new_bus) {
		err = -ENOMEM;
		goto err_free_priv;
	}

	new_bus->name = "Freescale PowerQUICC MII Bus",
	new_bus->read = &fsl_pq_mdio_read,
	new_bus->write = &fsl_pq_mdio_write,
	new_bus->reset = &fsl_pq_mdio_reset,
	new_bus->priv = priv;
	fsl_pq_mdio_bus_name(new_bus->id, np);
	addrp = of_get_address(np, 0, &size, NULL);
	if (!addrp) {
		err = -EINVAL;
		goto err_free_bus;
	}
	/* Set the PHY base address */
	addr = of_translate_address(np, addrp);
	if (addr == OF_BAD_ADDR) {
		err = -EINVAL;
		goto err_free_bus;
	}

	map = ioremap(addr, size);
	if (!map) {
		err = -ENOMEM;
		goto err_free_bus;
	}
	priv->map = map;

	if (of_device_is_compatible(np, "fsl,gianfar-mdio") ||
			of_device_is_compatible(np, "fsl,gianfar-tbi") ||
			of_device_is_compatible(np, "fsl,ucc-mdio") ||
			of_device_is_compatible(np, "ucc_geth_phy"))
		map -= offsetof(struct fsl_pq_mdio, miimcfg);
	regs = map;
	priv->regs = regs;

	new_bus->irq = kcalloc(PHY_MAX_ADDR, sizeof(int), GFP_KERNEL);

	if (NULL == new_bus->irq) {
		err = -ENOMEM;
		goto err_unmap_regs;
	}

	new_bus->parent = &ofdev->dev;
	dev_set_drvdata(&ofdev->dev, new_bus);

	if (of_device_is_compatible(np, "fsl,gianfar-mdio") ||
			of_device_is_compatible(np, "fsl,gianfar-tbi") ||
			of_device_is_compatible(np, "fsl,etsec2-mdio") ||
			of_device_is_compatible(np, "fsl,etsec2-tbi") ||
			of_device_is_compatible(np, "gianfar")) {
#if defined(CONFIG_GIANFAR) || defined(CONFIG_GIANFAR_MODULE)
		tbipa = get_gfar_tbipa(regs, np);
		if (!tbipa) {
			err = -EINVAL;
			goto err_free_irqs;
		}
#else
		err = -ENODEV;
		goto err_free_irqs;
#endif
	} else if (of_device_is_compatible(np, "fsl,ucc-mdio") ||
			of_device_is_compatible(np, "ucc_geth_phy")) {
#if defined(CONFIG_UCC_GETH) || defined(CONFIG_UCC_GETH_MODULE)
		u32 id;
		static u32 mii_mng_master;

		tbipa = &regs->utbipar;

		if ((err = get_ucc_id_for_range(addr, addr + size, &id)))
			goto err_free_irqs;

		if (!mii_mng_master) {
			mii_mng_master = id;
			ucc_set_qe_mux_mii_mng(id - 1);
		}
#else
		err = -ENODEV;
		goto err_free_irqs;
#endif
	} else {
		err = -ENODEV;
		goto err_free_irqs;
	}
	for_each_child_of_node(np, tbi) {
		if (!strncmp(tbi->type, "tbi-phy", 8))
			break;
	}

	if (tbi) {
		const u32 *prop = of_get_property(tbi, "reg", NULL);
		if (prop)
			tbiaddr = *prop;
	}
	if (tbiaddr == -1) {
		out_be32(tbipa, 0);

		tbiaddr = fsl_pq_mdio_find_free(new_bus);
	}

	/*
	 * We define TBIPA at 0 to be illegal, opting to fail for boards that
	 * have PHYs at 1-31, rather than change tbipa and rescan.
	 */
	if (tbiaddr == 0) {
		err = -EBUSY;

		goto err_free_irqs;
	}

	out_be32(tbipa, tbiaddr);

	err = of_mdiobus_register(new_bus, np);
	if (err) {
		printk (KERN_ERR "%s: Cannot register as MDIO bus\n",
				new_bus->name);
		goto err_free_irqs;
	}
/*add by zhangjj 2015-10-15*/
	if(!strcasecmp(new_bus->id,"mdio@ffe24000"))
	{
		preg = fsl_pq_mdio_get_regs(new_bus);
#ifdef BCM53101_C
		struct device *dev_n = &ofdev->dev;
		err = gpio_request(13, dev_name(dev_n));
		if (err) {
			dev_err(dev_n, "can't request gpio #%d: %d\n", 13, err);
		}
		err = gpio_request(5, dev_name(dev_n));
		if (err) {
			dev_err(dev_n, "can't request gpio #%d: %d\n", 5, err);
		}
		err = gpio_request(8, dev_name(dev_n));
		if (err) {
			dev_err(dev_n, "can't request gpio #%d: %d\n", 8, err);
		}
		
		BCM53101_C_init();
#endif
	}
/*add end*/

	return 0;

err_free_irqs:
	kfree(new_bus->irq);
err_unmap_regs:
	iounmap(priv->map);
err_free_bus:
	kfree(new_bus);
err_free_priv:
	kfree(priv);
	return err;
}


static int fsl_pq_mdio_remove(struct of_device *ofdev)
{
	struct device *device = &ofdev->dev;
	struct mii_bus *bus = dev_get_drvdata(device);
	struct fsl_pq_mdio_priv *priv = bus->priv;

	mdiobus_unregister(bus);

	dev_set_drvdata(device, NULL);

	iounmap(priv->map);
	bus->priv = NULL;
	mdiobus_free(bus);
	kfree(priv);

	return 0;
}

static struct of_device_id fsl_pq_mdio_match[] = {
	{
		.type = "mdio",
		.compatible = "ucc_geth_phy",
	},
	{
		.type = "mdio",
		.compatible = "gianfar",
	},
	{
		.compatible = "fsl,ucc-mdio",
	},
	{
		.compatible = "fsl,gianfar-tbi",
	},
	{
		.compatible = "fsl,gianfar-mdio",
	},
	{
		.compatible = "fsl,etsec2-tbi",
	},
	{
		.compatible = "fsl,etsec2-mdio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fsl_pq_mdio_match);

static struct of_platform_driver fsl_pq_mdio_driver = {
	.driver = {
		.name = "fsl-pq_mdio",
		.owner = THIS_MODULE,
		.of_match_table = fsl_pq_mdio_match,
	},
	.probe = fsl_pq_mdio_probe,
	.remove = fsl_pq_mdio_remove,
};

/*add by zhangjj*/
struct proc_dir_entry *mdio_dir, *mdiofile;

int proc_read_mdio(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
        return 1;
}

int proc_write_mdio(struct file *file, const char *buffer, unsigned long count, void *data) 
{
	return 0;
}
/*add end*/

int __init fsl_pq_mdio_init(void)
{
/*add by zhangjj 2015-9-25*/
        mdio_dir = proc_mkdir("mdio", NULL);
        mdiofile = create_proc_entry("option", S_IRUGO, mdio_dir);
        mdiofile->read_proc = proc_read_mdio;
        mdiofile->write_proc = proc_write_mdio;
/*add end*/
	return of_register_platform_driver(&fsl_pq_mdio_driver);
}
module_init(fsl_pq_mdio_init);

void fsl_pq_mdio_exit(void)
{
/*add by zhangjj 2015-9-25*/
        remove_proc_entry("mdio", mdio_dir);
        remove_proc_entry("option", NULL);
/*end*/
	of_unregister_platform_driver(&fsl_pq_mdio_driver);
}
module_exit(fsl_pq_mdio_exit);
MODULE_LICENSE("GPL");
