#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>	//for copy_to_user
#include <linux/gpio.h>

struct cdev bcm53101_cdev;

struct fsl_pq_mdio {
	u8 res1[16];
	u32 ieventm;	/* MDIO Interrupt event register (for etsec2)*/
	u32 imaskm;	/* MDIO Interrupt mask register (for etsec2)*/
	u8 res2[4];
	u32 emapm;	/* MDIO Event mapping register (for etsec2)*/
	u8 res3[1280];
	u32 miimcfg;		/* MII management configuration reg */
	u32 miimcom;		/* MII management command reg */
	u32 miimadd;		/* MII management address reg */
	u32 miimcon;		/* MII management control reg */
	u32 miimstat;		/* MII management status reg */
	u32 miimind;		/* MII management indication reg */
	u8 reserved[28];	/* Space holder */
	u32 utbipar;		/* TBI phy address reg (only on UCC) */
	u8 res4[2728];
} __attribute__ ((packed));

struct fsl_pq_mdio __iomem *preg = NULL;

extern int bcm53101_get_preg(struct fsl_pq_mdio **upreg);
extern int fsl_pq_local_mdio_read(struct fsl_pq_mdio __iomem *regs, int mii_id, int regnum);
extern int fsl_pq_local_mdio_write(struct fsl_pq_mdio __iomem *regs, int mii_id, int regnum, u16 value);

#define PSEPHY_ACCESS_CTRL	16
#define PSEPHY_RDWR_CTRL	17
#define PSEPHY_ACCESS_REG1	24
#define PSEPHY_ACCESS_REG2	25
#define PSEPHY_ACCESS_REG3	26
#define PSEPHY_ACCESS_REG4	27

#define PSEDOPHY		30
#define PHY0			0
#define PHY1			1
#define PHY2			2
#define PHY3			3
#define PHY4			4

#define ACCESS_EN		1

#define STRINGLEN 100
char global_buffer[STRINGLEN];

#define OPER_RD 0x2
#define OPER_WR 0x1
int readFlag=0;
int phyaddr = 2;

struct bcm53101_t{
	unsigned char  page;
	unsigned char  addr;
	unsigned short val[4];
};

int fsl_mdio_write(int mii_id, unsigned short addr, unsigned short val)
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
int fsl_mdio_read(int mii_id, unsigned short addr, unsigned short *val)
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
int bcm53101_write(unsigned char page, unsigned char addr, unsigned short *value)
{
        unsigned short mdio_val = 0;
        unsigned short ret_val = 0, s_count = 0xffff;
        int mii_id = 0;

        if(((page >= 0x10) && (page <= 0x14))||(page == 0x17))//get real port phy addr
        {
                mii_id = page - 0x10;
//		printk("mii_id=%d\n",mii_id);
                fsl_mdio_write(mii_id, addr/2, value[0]);
        }
        else                            //get psedophy addr
        {
                mii_id = PSEDOPHY;
//		printk("mii_id=%d\n",mii_id);
		page &= 0xff;
		mdio_val |= (page << 8);
	        mdio_val |= ACCESS_EN;
	
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_CTRL,mdio_val);
	
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG1,value[0]);
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG2,value[1]);
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG3,value[2]);
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_REG4,value[3]);

		mdio_val = 0;
	        mdio_val |= addr << 8;
	        mdio_val |= OPER_WR;
	        fsl_mdio_write(mii_id, PSEPHY_RDWR_CTRL,mdio_val);

//	msleep(1);	
		do
		{
			fsl_mdio_read(mii_id, PSEPHY_RDWR_CTRL, &mdio_val);
			if(!(mdio_val&0x11))
				break;
			s_count --;
	//		msleep(1);
		}while(s_count > 0);
	}
	return 0;
}
int bcm53101_fs_write(struct file *filp, const char __user *buf,
                size_t count, loff_t *f_pos)
{
	struct bcm53101_t bcmstru;
	unsigned char page, addr;	
	unsigned short value[4];

	copy_from_user(&bcmstru, buf, count);
	page = bcmstru.page;
	addr = bcmstru.addr;
	memcpy(value, bcmstru.val, sizeof(value));
	bcm53101_write(page, addr, value);

	return 0;
}

int bcm53101_read(unsigned char page, unsigned char addr, unsigned short *value)
{
        unsigned short mdio_val = 0;
        unsigned short setval, ret_val = 0, s_count = 0xffff;
	int mii_id = 0;	

printk("page=0x%x,addr=0x%x\n",page,addr);

	if(((page >= 0x10) && (page <= 0x14))||(page == 0x17))//get real port phy addr
	{
		mii_id = page - 0x10;
//		printk("mii_id=%d\n",mii_id);
		fsl_mdio_read(mii_id, addr/2, &mdio_val);
		value[3] = 0; 
		value[2] = 0; 
		value[1] = 0; 
		value[0] = mdio_val; 
	}
	else				//get psedophy addr
	{
		mii_id = PSEDOPHY;
//		printk("mii_id=%d\n",mii_id);
	        page &= 0xff;
		setval = 0;
	        setval |= (page << 8);
	        setval |= ACCESS_EN;
	        fsl_mdio_write(mii_id, PSEPHY_ACCESS_CTRL,setval);
		
	        setval = 0;
	        setval |= addr << 8;
	        setval |= OPER_RD;
	        fsl_mdio_write(mii_id, PSEPHY_RDWR_CTRL,setval);
	
	        do
	        {
	                ret_val = fsl_mdio_read(mii_id, PSEPHY_RDWR_CTRL, &setval);
	                if((!(setval&0x11))||(ret_val))
	                        break;
			s_count --;
	//		msleep(1);
	        }while(s_count > 0);
	        ret_val = fsl_mdio_read(mii_id, PSEPHY_ACCESS_REG1, &setval);
		value[0] = setval;
		printk("value[0]=0x%x\n",value[0]);
	        ret_val = fsl_mdio_read(mii_id, PSEPHY_ACCESS_REG2, &setval);
		value[1] = setval;
		printk("value[1]=0x%x\n",value[1]);
	        ret_val = fsl_mdio_read(mii_id, PSEPHY_ACCESS_REG3, &setval);
		value[2] = setval;
		printk("value[2]=0x%x\n",value[2]);
	        ret_val = fsl_mdio_read(mii_id, PSEPHY_ACCESS_REG4, &setval);
		value[3] = setval;
		printk("value[3]=0x%x\n",value[3]);
		if(ret_val)
			return -1;
	}

	return 0;
#if 0
//        printk("24: 0x%x\n",mdio_val);
	        ret_val = fsl_pq_local_mdio_read(preg,30,25);
	        printk("25: 0x%x\n",ret_val);
	        ret_val = fsl_pq_local_mdio_read(preg,30,26);
	        printk("26: 0x%x\n",ret_val);
	        ret_val = fsl_pq_local_mdio_read(preg,30,27);
	        printk("27: 0x%x\n",ret_val);
#endif
}
int bcm53101_fs_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct bcm53101_t bcmstru;
	unsigned char page, addr;	
	unsigned short value[4];

	copy_from_user(&bcmstru, buf, count);
	page = bcmstru.page;
	addr = bcmstru.addr;
	printk("addr0x%x, bcmstru.addr=%x\n",addr,bcmstru.addr);
	bcm53101_read(page, addr, value);
	memcpy(bcmstru.val, value, sizeof(value));
	copy_to_user(buf, &bcmstru, sizeof(bcmstru));

	return 0;
}

#define BCM53101_A 0
#define BCM53101_B 1
#define BCM53101_GPIO_SEL	8

static long bcm53101_fs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int  err = 0;
        int  retval = 0;

	switch (cmd)
        {
                case BCM53101_A:
        		gpio_direction_output(BCM53101_GPIO_SEL, BCM53101_A);
                        break;

                case BCM53101_B:
		        gpio_direction_output(BCM53101_GPIO_SEL, BCM53101_B);
                        break;
                default:
                 retval = -EINVAL;
                break;
        }
        return retval;
}

static struct file_operations bcm53101_fops = {
	.owner = THIS_MODULE,
	.read  = bcm53101_fs_read,
	.write = bcm53101_fs_write,
	.unlocked_ioctl = bcm53101_fs_ioctl
};


int bcm53101_cfg_init()
{
	unsigned short mdio_val[4] = {0};
	unsigned char i = 0;

	if(0 != bcm53101_get_preg(&preg))
	{
		printk("Get preg error\n");
		return 0;	
	}
#if 0
	for(i = 0; i < 2; i++)
	{
		gpio_direction_output(BCM53101_GPIO_SEL, i);
	        memset(mdio_val, 0, sizeof(mdio_val));
	        mdio_val[0] = 0x80;
	//        bcm53101_write(2, 0, mdio_val);
	        printk("write 2 0 0x80\n\n");
	
	        memset(mdio_val, 0, sizeof(mdio_val));
	        bcm53101_read(0, 0xe, mdio_val);
	        printk("read 0,0e:0x%x\n\n",mdio_val[0]);
	
	        memset(mdio_val, 0, sizeof(mdio_val));
	        mdio_val[0] = 0x8b;
	        bcm53101_write(0, 0xe, mdio_val);
	        printk("write 0 0e 0x8b\n\n");
	
	        memset(mdio_val, 0, sizeof(mdio_val));
	        bcm53101_read(0, 0xe, mdio_val);
	        printk("read 0,0e:0x%x\n\n",mdio_val[0]);
	
	        memset(mdio_val, 0, sizeof(mdio_val));
	        bcm53101_read(1, 0, mdio_val);
	        printk("read 1,0:0x%x\n",mdio_val[0]);
	}
#endif
	gpio_direction_output(BCM53101_GPIO_SEL, BCM53101_A);

	//set IMP port enable 
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x80;
	bcm53101_write(2, 0x0, mdio_val);

	//set IMP port receive uni/multi/broad cast enable 
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x1c;
	bcm53101_write(0, 0x08, mdio_val);

	//disable broad header for IMP port
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x0;
	bcm53101_write(2, 0x03, mdio_val);

	//set Switch Mode forwarding enable and managed mode
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x3;
	bcm53101_write(0, 0x0b, mdio_val);

	//set IMP Port State 1000M duplex and Link pass
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x8b;
	bcm53101_write(0, 0x0e, mdio_val);

	//set IMP RGMII RX/TX clock delay enable
	memset(mdio_val, 0, sizeof(mdio_val));
	mdio_val[0] = 0x3;
	bcm53101_write(0, 0x60, mdio_val);

	return 0;
}

static struct class *bcm53101_cls;
int major;
static int __init bcm53101_init(void)
{
        int status = 0;
	dev_t dev_id;

     	printk(KERN_ALERT "BCM53101 driver init...");

	alloc_chrdev_region(&dev_id, 0, 1, "bcm53101");
	major = MAJOR(dev_id);
	
	cdev_init(&bcm53101_cdev, &bcm53101_fops);
	cdev_add(&bcm53101_cdev, dev_id, 1);

	bcm53101_cls = class_create(THIS_MODULE, "bcm53101");

	device_create(bcm53101_cls, NULL, dev_id, NULL, "bcm53101");	


	bcm53101_cfg_init();

     	printk(KERN_ALERT "Done\n");

     	return 0;
}

static void __exit bcm53101_exit(void)
{
     	printk(KERN_ALERT "bcm53101 driver exit\n");

	device_destroy(bcm53101_cls, MKDEV(major, 0));
    	class_destroy(bcm53101_cls);
 
	/* 3.3 删除cdev*/
	cdev_del(&bcm53101_cdev);
 
	/* 3.4 释放设备号*/
	unregister_chrdev_region(MKDEV(major, 0), 1);
}

module_init(bcm53101_init);
module_exit(bcm53101_exit);
MODULE_LICENSE("GPL");


