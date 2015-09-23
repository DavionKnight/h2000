/* driver/tdm/tdm-dev.c
 *
 * Copyright (C) 2010 Freescale Semiconductor, Inc, All rights reserved.
 *
 * tdm-dev.c - tdm-bus driver, char device interface
 *
 * Author:Hemant Agrawal <hemant@freescale.com>
 * 	Rajesh Gumasta <rajesh.gumasta@freescale.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/tdm.h>
#include <linux/tdm-dev.h>
#include <linux/smp_lock.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>

#define TDM_FRAME_LENGTH NUM_OF_FRAMES
static struct tdm_driver tdmdev_driver;
static struct tasklet_struct tdmdev_stats_tasklet;

static void print_port_stats(struct tdm_port *p_tdm_port,
			struct tdm_port_stats *port_stats)
{
	pr_info("\tChannel-%d (minor=%d) statistics:\n \
				rx_pkt_count	:%u\n \
				rx_pkt_drop_count	:%u\n \
				tx_pkt_count	:%u\n \
				tx_pkt_drop_count	:%u\n \
				txPktConf	:%u",
		 p_tdm_port->ch_id, p_tdm_port->first_slot,
		 port_stats->rx_pkt_count,
		 port_stats->rx_pkt_drop_count,
		 port_stats->tx_pkt_count,
		 port_stats->tx_pkt_drop_count,
		 port_stats->tx_pkt_conf_count);
}

static void dump_tdm_stats(unsigned long data)
{
/* todo implement it for the list of ports this driver maintains */
/*     	struct tdm_port *h_port;
 *      struct tdm_port_stats	*port_stat;
 *     	tdm_port_get_stats(h_port,port_stat);
 * 	print_port_stats(h_port,port_stat);
*/
}

static int tdm_proc_dump_stats(char *buffer, char **start, off_t offset,
				 int length, int *eof, void *data)
{
	char *next = buffer;
	unsigned size = length;
	int t;

	t = scnprintf(next, size, "Freescale tdm driver statistics.\n");
	size -= t;
	next += t;
	tasklet_schedule(&tdmdev_stats_tasklet);

	*eof = 1;
	return length - size;
}


static int tdmdev_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	int err = TDM_E_OK;
	void *handle;

	pr_debug("OPEN Port- %d\n", minor);

	if (file->private_data != NULL)
		return -ENXIO;

	if ((minor < 0) || (minor >= TDM_ACTIVE_CHANNELS)) {
		pr_err("%s-minor number(%d)out of range\n", __func__, minor);
		return -EINVAL;
	}

	err = tdm_port_open(&tdmdev_driver, minor, &handle);
	if (err != TDM_E_OK) {
		pr_err("Error in tdm_port_open(%d)- err %x\n", minor, err);
		return -ENXIO;
	}

	file->private_data = handle;

	return err;
}

static int tdmdev_close(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	int err = TDM_E_OK;

	pr_debug("tdm_close(%p,%p)\n", inode, file);

	if (!file->private_data) {
		pr_err("%s-chan-%d-Null file pointer\n", __func__, minor);
		return -ENXIO;
	}

	if ((minor < 0) || (minor >= TDM_ACTIVE_CHANNELS)) {
		pr_err("%s-minor number(%d)out of range\n", __func__, minor);
		return -EINVAL;
	}

	err = tdm_port_close((void *)file->private_data);
	if (err != TDM_E_OK) {
		pr_debug("Error in tdm_port_Close\n");
		return -ENXIO;
	}

	file->private_data = NULL;

	return err;
}

static long tdmdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	if (!file->private_data) {
		pr_err("%s--Null file pointer\n", __func__);
		return -ENXIO;
	}

	err = tdm_port_ioctl((void *)file->private_data, cmd, arg);
	if (err != TDM_E_OK) {
		pr_err("Error in tdm_port_ioctl\n");
		return -ENXIO;
	}

	return err;

}


static ssize_t tdmdev_read(struct file *file, char *data_buffer, size_t length,
				loff_t *off)
{
	int err;
	unsigned short p_data[TDM_FRAME_LENGTH];
	uint16_t size = 0;

	pr_debug("%s ", __func__);

	if (!file->private_data) {
		pr_err("%s--Null file pointer\n", __func__);
		return -ENXIO;
	}

	err = tdm_port_read((void *)file->private_data, &p_data, &size);
	if (err != TDM_E_OK) {
		pr_debug("Error in tdm_port_read\n");
		return -ENXIO;
	}

	err = copy_to_user((u16 *)data_buffer, p_data, length * 2);
	if (err > 0) {
		pr_err("Error in copy_to_user:%d bytes not copied", err);
		return -EFAULT;
	}

	return size;
}

static ssize_t tdmdev_write(struct file *file, const char *data_buffer,
				size_t length, loff_t *off)
{
	int err = TDM_E_OK;
	unsigned short local_buffer[TDM_FRAME_LENGTH];
	unsigned int size = 0;

	pr_debug("%s ", __func__);

	if (!file->private_data) {
		pr_err("%s--Null file pointer\n", __func__);
		return -EFAULT;
	}

	size = length;
	if (length > TDM_FRAME_LENGTH) {
		pr_err("Bufer Length is More than Maximum Allowed Length"
			"(%d)<%d\n", TDM_FRAME_LENGTH, length);
		return -ENXIO;
	}

	err = copy_from_user((void *)local_buffer, (u16 *)data_buffer,
			length * 2);
	if (err != TDM_E_OK) {
		pr_err("Error in copy_from_user:%d bytes not copied", err);
		return -EFAULT;

	}

	err = tdm_port_write((void *)file->private_data, local_buffer, size);
	if (err != TDM_E_OK) {
		pr_err("Error in tdm_port_write\n");
		return -EFAULT;
	}

	return size;
}

static unsigned int tdmdev_poll(struct file *file, poll_table *wait)
{
	int err = TDM_E_OK;
	int poll_time = 13;

	pr_debug("%s ", __func__);

	if (!file->private_data) {
		pr_err("%s--Null file pointer\n", __func__);
		return -ENXIO;
	}

	/* This function can be called by two process but on different
	 minor device so locking is not required */
	/* todo implement it using poll_wait */
	err = tdm_port_poll((void *)file->private_data, poll_time);
	if (err == TDM_E_OK) {
		pr_debug("Data Available on Port\n");
		return POLLIN | POLLRDNORM;
	}

	return -EINVAL;
}

static const struct file_operations tdmdev_fops = {
	.owner		= THIS_MODULE,
	.read		= tdmdev_read,
	.write		= tdmdev_write,
	.unlocked_ioctl	= tdmdev_ioctl,
	.open		= tdmdev_open,
	.release	= tdmdev_close,
	.poll		= tdmdev_poll,
};

static struct class *tdm_dev_class;

static int tdmdev_attach_adapter(struct tdm_adapter *adap)
{
	pr_debug("tdm-dev: attached to adapter [%s] id = %d\n",
		 adap->name, adap->id);
	return TDM_E_OK;
}

static int tdmdev_detach_adapter(struct tdm_adapter *adap)
{
	pr_debug("tdm-dev: adapter [%s] unregistered\n", adap->name);
	return TDM_E_OK;
}


static int tdm_linux_register(void)
{
	struct proc_dir_entry  *proc_tdm, *proc_tdm_stats;
	int res = 0;

	res = register_chrdev(TDM_MAJOR_NUM, TDM_DEV_NAME, &tdmdev_fops);
	if (res != 0) {
		pr_err("TDM MAJOR no. %d Unable to register devfs",
				TDM_MAJOR_NUM);
		return -ENXIO;
	}

	/* Creating proc for tdm stats and register dumps*/
	if (((proc_tdm = proc_mkdir("tdm", NULL)) == NULL) ||
		((proc_tdm_stats = create_proc_read_entry("tdm-stats", 0, 0,
					tdm_proc_dump_stats, NULL)) == NULL)) {
		unregister_chrdev(TDM_MAJOR_NUM, TDM_DEV_NAME);
		pr_err("TDM_MAJOR_NUM %d Unable to create proc read entry -"
				"proc/tdm/regs", TDM_MAJOR_NUM);
		return -EINVAL;
	}
	tasklet_init(&tdmdev_stats_tasklet, dump_tdm_stats, 0);

	return TDM_E_OK;
}

static int tdm_linux_unregister(void)
{
	tasklet_kill(&tdmdev_stats_tasklet);

	/* Remove the proc entry */
	remove_proc_entry("tdm-stats", NULL);
	remove_proc_entry("tdm", NULL);

	unregister_chrdev(TDM_MAJOR_NUM, TDM_DEV_NAME);

	return TDM_E_OK;
}


static const struct tdm_device_id starlite_id[] = {
	{ "fsl_starlite", 0 },
	{ }
};

/*
 * The legacy "tdmdev_driver" is used primarily to get notifications when
 * TDM adapters are added or removed, so that each one gets an tdm_dev
 * and is thus made available to userspace driver code.
 */
static struct tdm_driver tdmdev_driver = {
	.attach_adapter = tdmdev_attach_adapter,
	.detach_adapter = tdmdev_detach_adapter,
	.id_table = starlite_id,
};

/* module load/unload record keeping */
static int __init tdm_dev_init(void)
{
	int res;

	pr_info(KERN_INFO "mknod tdm c %d 0\n", TDM_MAJOR_NUM);

	tdm_dev_class = class_create(THIS_MODULE, "tdm-dev");
	if (IS_ERR(tdm_dev_class)) {
		res = PTR_ERR(tdm_dev_class);
		goto out;
	}

	tdmdev_driver.id = 2;

	res = tdm_add_driver(&tdmdev_driver);
	if (res)
		goto out_unreg_class;

	if (tdm_linux_register() != TDM_E_OK) {
		pr_err("Failed to Register the Linux Device!\n");
		return -ENODEV;
	}

	pr_info("%s: registering the character device adap %x\n", __func__,
			tdmdev_driver.adapter);
	return 0;

out_unreg_class:
	class_destroy(tdm_dev_class);
out:
	pr_err("%s: registering the character device failed\n", __func__);
	return res;
}

static void __exit tdm_dev_exit(void)
{
	tdm_linux_unregister();

	pr_info("%s: unregistering the character device\n", __func__);
	tdm_del_driver(&tdmdev_driver);
	class_destroy(tdm_dev_class);
}

MODULE_AUTHOR("Hemant Agrawal <hemant@freescale.com> and "
	"Rajesh Gumasta <rajesh.gumasta@freescale.com>");
MODULE_DESCRIPTION("TDM /dev entries driver");
MODULE_LICENSE("GPL");

module_init(tdm_dev_init);
module_exit(tdm_dev_exit);
