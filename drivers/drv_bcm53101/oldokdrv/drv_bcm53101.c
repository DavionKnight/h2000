#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>

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

#define MII_ID			30
#define ACCESS_EN		1

#define STRINGLEN 100
char global_buffer[STRINGLEN];

#define OPER_RD 0x2
#define OPER_WR 0x1
int readFlag=0;
int phyaddr = 2;


int fsl_mdio_write(unsigned short addr, unsigned short val)
{
	int ret = 0;

	if(NULL == preg)
	{
		printk("preg is NULL\n");
		return -1;
	}

	ret = fsl_pq_local_mdio_write(preg, MII_ID, addr, val);

	return ret;
}
int fsl_mdio_read(unsigned short addr, unsigned short *val)
{
        int ret = 0;

        if(NULL == preg)
        {
                printk("preg is NULL\n");
                return -1;
        }

        ret = fsl_pq_local_mdio_read(preg, MII_ID, addr);
	*val = ret & 0xffff;
	
        return 0;

}

int bcm53101_write(unsigned char page, unsigned char addr, unsigned short value)
{
        unsigned short mdio_val = 0;
        unsigned short ret_val = 0, count = 0xffff;
	
	page &= 0xff;
	mdio_val |= (page << 8);
        mdio_val |= ACCESS_EN;

        fsl_mdio_write(PSEPHY_ACCESS_CTRL,mdio_val);

        mdio_val = value;
        fsl_mdio_write(PSEPHY_ACCESS_REG1,mdio_val);
#if 0
        mdio_val = 0;
        fsl_mdio_write(PSEPHY_ACCESS_REG2,mdio_val);
        mdio_val = 0;
        fsl_mdio_write(PSEPHY_ACCESS_REG3,mdio_val);
        mdio_val = 0;
        fsl_mdio_write(PSEPHY_ACCESS_REG4,mdio_val);
#endif

	mdio_val = 0;
        mdio_val |= addr << 8;
        mdio_val |= OPER_WR;
        fsl_mdio_write(PSEPHY_RDWR_CTRL,mdio_val);

//	msleep(1);	
	do
	{
		fsl_mdio_read(PSEPHY_RDWR_CTRL, &mdio_val);
		if(!(mdio_val&0x11))
			break;
		count --;
//		msleep(1);
	}while(count > 0);
	return 0;
}

int bcm53101_read(unsigned char page, unsigned char addr, unsigned short *value)
{
        unsigned short mdio_val = 0;
        unsigned short ret_val = 0, count = 0xffff;

        page &= 0xff;
        mdio_val |= (page << 8);
        mdio_val |= ACCESS_EN;
        fsl_mdio_write(PSEPHY_ACCESS_CTRL,mdio_val);
	
        mdio_val = 0;
        mdio_val |= addr << 8;
        mdio_val |= OPER_RD;
        fsl_mdio_write(PSEPHY_RDWR_CTRL,mdio_val);

        do
        {
                ret_val = fsl_mdio_read(PSEPHY_RDWR_CTRL, &mdio_val);
                if((!(mdio_val&0x11))||(ret_val))
                        break;
		count --;
//		msleep(1);
        }while(count > 0);
        ret_val = fsl_mdio_read(PSEPHY_ACCESS_REG1, &mdio_val);
	if(ret_val)
		return -1;
	*value = mdio_val;
//        printk("24: 0x%x\n",mdio_val);
#if 0
        ret_val = fsl_pq_local_mdio_read(preg,30,25);
        printk("25: 0x%x\n",ret_val);
        ret_val = fsl_pq_local_mdio_read(preg,30,26);
        printk("26: 0x%x\n",ret_val);
        ret_val = fsl_pq_local_mdio_read(preg,30,27);
        printk("27: 0x%x\n",ret_val);
#endif
	return 0;
}


static int __init bcm53101_init(void)
{
     	printk(KERN_ALERT "driver init!\n");

	if(0 != bcm53101_get_preg(&preg))
	{
		printk("Get preg error\n");
		return 0;	
	}

	unsigned short mdio_val = 0;
#if 1
	bcm53101_write(2, 0, 0x80);	
	printk("1\n");
	bcm53101_write(0, 0xe, 0x8b);
	printk("2\n");
	bcm53101_read(0, 0xe, &mdio_val);
	printk("0,0e:0x%x\n",mdio_val);
	bcm53101_read(1, 0, &mdio_val);
	printk("1,0:0x%x\n",mdio_val);

#else

	unsigned short ret_val = 0;	


	int i;
	if(readFlag == 0)
	{
	/* config 0200 reg to 0x80 */
	mdio_val = (2 << 8);
	mdio_val |= 1;

	fsl_pq_local_mdio_write(preg,30,16,mdio_val);

	mdio_val = 0x80;
	fsl_pq_local_mdio_write(preg,30,24,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,25,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,26,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,27,mdio_val);

	mdio_val = 0;
	mdio_val |= OPER_WR;
	fsl_pq_local_mdio_write(preg,30,17,mdio_val);

	for(i=0; i<0xffff; i++);

	/* config 000e reg to 0x8b */
	mdio_val = 0; 
	mdio_val |= 1;

	fsl_pq_local_mdio_write(preg,30,16,mdio_val);

	mdio_val = 0x8b;
	fsl_pq_local_mdio_write(preg,30,24,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,25,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,26,mdio_val);
	mdio_val = 0;
	fsl_pq_local_mdio_write(preg,30,27,mdio_val);

	mdio_val = 0;
	mdio_val |= 0x0e<<8;
	mdio_val |= OPER_WR;
	fsl_pq_local_mdio_write(preg,30,17,mdio_val);

	for(i=0; i<0xffff; i++);
	readFlag = 0xa5;
	}


	mdio_val |= 0x1;
	fsl_pq_local_mdio_write(preg,30,16,mdio_val);

	mdio_val = 0;
	mdio_val |= 0x0e<<8;
	//mdio_val |= 0x0b<<8;
	mdio_val |= OPER_RD;
	fsl_pq_local_mdio_write(preg,30,17,mdio_val);

read:
	ret_val = fsl_pq_local_mdio_read(preg,30,17);
	if(ret_val&0x11)
	{
		printk("ret_val = 0x%x\n",ret_val);
		goto read;
	}
	
	ret_val = fsl_pq_local_mdio_read(preg,30,24);
	printk("24: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,25);
	printk("25: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,26);
	printk("26: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,27);
	printk("27: 0x%x\n",ret_val);

	mdio_val = (1 << 8);
	
	mdio_val |= 0x1;
	fsl_pq_local_mdio_write(preg,30,16,mdio_val);

	mdio_val = 0;
	//mdio_val |= 0x04<<8;
	mdio_val |= OPER_RD;
	fsl_pq_local_mdio_write(preg,30,17,mdio_val);

read2:
	ret_val = fsl_pq_local_mdio_read(preg,30,17);
	if(ret_val&0x11)
	{
		printk("ret_val2 = 0x%x\n",ret_val);
		goto read2;
	}
	
	ret_val = fsl_pq_local_mdio_read(preg,30,24);
	printk("24: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,25);
	printk("25: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,26);
	printk("26: 0x%x\n",ret_val);
	ret_val = fsl_pq_local_mdio_read(preg,30,27);
	printk("27: 0x%x\n",ret_val);
#endif
     	return 0;
}

static void __exit bcm53101_exit(void)
{
     printk(KERN_ALERT "bcm53101 driver exit\n");
}

module_init(bcm53101_init);
module_exit(bcm53101_exit);



