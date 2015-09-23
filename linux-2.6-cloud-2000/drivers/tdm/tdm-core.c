/* driver/tdm/tdm-core.c
 *
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc, All rights reserved.
 *
 * TDM core is the interface between TDM ports and devices.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tdm.h>
#include <linux/init.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/hardirq.h>
#include <linux/irqflags.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <linux/io.h>

/*
 * core_lock protects tdm_adapter_idr, and guarantees that device,
 * deletion of devices, and attach_adapter and detach_adapter calls
 * are serialized */
static DEFINE_MUTEX(core_lock);
static DEFINE_IDR(tdm_adapter_idr);
LIST_HEAD(adapter_list);
LIST_HEAD(driver_list);

/* In case the previous data is not fetched by the client driver, the
 * de-interleaving function will  discard the old data and rewrite the
 * new data */
static int use_latest_tdm_data = 1;

static void tdm_data_tasklet_fn(unsigned long);

static int tdm_device_match(struct tdm_driver *driver, struct tdm_adapter *adap)
{
	/* match on an id table if there is one */
	if (driver->id_table && driver->id_table->name[0]) {
		if (!(strcmp(driver->id_table->name, adap->name)))
			return (int)driver->id_table;
	}
	return TDM_E_OK;
}

static int tdm_do_add_adapter(struct tdm_driver *driver,
					struct tdm_adapter *adap)
{
	/* if driver is already attached to any other adapter, return*/
	if (driver->adapter && driver->adapter != adap)
		return TDM_E_OK;

	driver->adapter = adap;

	if (driver->attach_adapter) {
		/* We ignore the return code; if it fails, too bad */
		driver->attach_adapter(adap);
	}
	adap->drv_count++;

	if (!adap->tasklet_conf) {
		tasklet_init(&adap->tdm_data_tasklet, tdm_data_tasklet_fn,
						(unsigned long)adap);
		adap->tasklet_conf = 1;
	}

	return TDM_E_OK;
}

static int tdm_register_adapter(struct tdm_adapter *adap)
{
	int res = TDM_E_OK;
	struct tdm_driver *driver, *next;

	mutex_init(&adap->adap_lock);
	INIT_LIST_HEAD(&adap->myports);
	spin_lock_init(&adap->portlist_lock);

	adap->drv_count = 0;
	adap->tasklet_conf = 0;

	list_add_tail(&adap->list, &adapter_list);

	/* Notify drivers */
	pr_info("adapter [%s] registered\n", adap->name);
	mutex_lock(&core_lock);
	list_for_each_entry_safe(driver, next, &driver_list, list) {
		if (tdm_device_match(driver, adap)) {
			res = tdm_do_add_adapter(driver, adap);
			pr_info(
			"Driver(ID=%d) is attached with Adapter %s(ID = %d)\n",
				driver->id, adap->name, adap->id);
		}
	}
	mutex_unlock(&core_lock);

	return res;
}

/*
 * tdm_add_adapter - declare tdm adapter, use dynamic device number
 * @adapter: the adapter to add
 * Context: can sleep
 *
 * This routine is used to declare an TDM adapter
 * When this returns zero, a new device number will be allocated and stored
 * in adap->id, and the specified adapter became available for ports.
 * Otherwise, a negative errno value is returned.
 */
int tdm_add_adapter(struct tdm_adapter *adapter)
{
	int id, res = TDM_E_OK;

retry:
	if (idr_pre_get(&tdm_adapter_idr, GFP_KERNEL) == 0)
		return -ENOMEM;

	mutex_lock(&core_lock);
	res = idr_get_new(&tdm_adapter_idr, adapter, &id);
	mutex_unlock(&core_lock);

	if (res < 0) {
		if (res == -EAGAIN)
			goto retry;
		return res;
	}

	adapter->id = id;
	return tdm_register_adapter(adapter);
}
EXPORT_SYMBOL(tdm_add_adapter);

static int tdm_do_del_adapter(struct tdm_driver *driver,
					struct tdm_adapter *adap)
{
	int res = TDM_E_OK;

	if (!driver->adapter || driver->adapter != adap)
		return TDM_E_OK;

	if (!driver->detach_adapter)
		return TDM_E_OK;

	adap->drv_count--;

	res = driver->detach_adapter(adap);
	if (res)
		pr_err("detach_adapter failed (%d) "
			"for driver [%s]\n", res, driver->name);
	driver->adapter = NULL;
	return res;
}

/**
 * tdm_del_adapter - unregister TDM adapter
 * @adap: the adapter being unregistered
 * Context: can sleep
 *
 * This unregisters an TDM adapter which was previously registered
 * by @tdm_add_adapter.
 */
int tdm_del_adapter(struct tdm_adapter *adap)
{
	int res = TDM_E_OK;
	struct tdm_adapter *found;
	struct tdm_driver *driver, *next;

	/* First make sure that this adapter was ever added */
	mutex_lock(&core_lock);
	found = idr_find(&tdm_adapter_idr, adap->id);
	mutex_unlock(&core_lock);
	if (found != adap) {
		pr_err("tdm-core: attempting to delete unregistered "
			 "adapter [%s]\n", adap->name);
		return -EINVAL;
	}

	/*disable and kill the data processing tasklet */
	if (adap->tasklet_conf) {
		tasklet_disable(&adap->tdm_data_tasklet);
		tasklet_kill(&adap->tdm_data_tasklet);
		adap->tasklet_conf = 0;
	}

	/* Detach any active ports. This can't fail, thus we do not
	   checking the returned value. */
	mutex_lock(&core_lock);
	list_for_each_entry_safe(driver, next, &driver_list, list) {
		if (tdm_device_match(driver, adap)) {
			tdm_do_del_adapter(driver, adap);
			pr_info(
			"Driver(ID=%d) is detached from Adapter %s(ID = %d)\n",
				 driver->id, adap->name, adap->id);
		}
	}
	mutex_unlock(&core_lock);

	mutex_lock(&core_lock);
	idr_remove(&tdm_adapter_idr, adap->id);
	mutex_unlock(&core_lock);

	pr_debug("adapter [%s] unregistered\n", adap->name);

	list_del(&adap->list);
	/* Clear the device structure in case this adapter is ever going to be
	   added again */
	adap->parent = NULL;

	return res;
}
EXPORT_SYMBOL(tdm_del_adapter);

static int __attach_adapter(struct tdm_adapter *adap, void *data)
{
	struct tdm_driver *driver = (struct tdm_driver *)data;

	driver->adapter = adap;

	if (driver->attach_adapter)
		driver->attach_adapter(adap);

	if (!adap->tasklet_conf) {
		tasklet_init(&adap->tdm_data_tasklet, tdm_data_tasklet_fn,
				(unsigned long)adap);
		adap->tasklet_conf = 1;
	}
	adap->drv_count++;

	return TDM_E_OK;
}

/*
 * An tdm_driver is used with one or more tdm_port nodes to access
 * tdm device, on a device instance associated with some tdm_adapter.
 */

int tdm_register_driver(struct module *owner, struct tdm_driver *driver)
{
	int res = TDM_E_OK;
	struct tdm_adapter *adap, *next;

	list_add_tail(&driver->list, &driver_list);

	mutex_lock(&core_lock);
	/* Walk the adapters that are already present */
	list_for_each_entry_safe(adap, next, &adapter_list, list) {
		if (tdm_device_match(driver, adap)) {
			res = __attach_adapter(adap, driver);
			pr_info("TDM Driver(ID=%d)is attached with Adapter"
				"%s(ID = %d) drv_count=%d", driver->id,
				adap->name, adap->id, adap->drv_count);
		break;
		}
	}
	mutex_unlock(&core_lock);

	return res;
}
EXPORT_SYMBOL(tdm_register_driver);

static int __detach_adapter(struct tdm_adapter *adap, void *data)
{
	struct tdm_driver *driver = (struct tdm_driver *)data;

	if (adap == NULL)
		return -ENODEV;

	adap->drv_count--;

	/* If no more driver is registed with the adapter*/
	if (!adap->drv_count && adap->tasklet_conf) {
		tasklet_disable(&adap->tdm_data_tasklet);
		tasklet_kill(&adap->tdm_data_tasklet);
		adap->tasklet_conf = 0;
	}

	if (driver->detach_adapter) {
		if (driver->detach_adapter(adap))
			pr_err("detach_adapter failed for driver [%s]\n",
				driver->name);
	}
	driver->adapter = NULL;

	return TDM_E_OK;
}

/*
 * tdm_del_driver - unregister TDM driver
 * @driver: the driver being unregistered
 * Context: can sleep
 */
void tdm_del_driver(struct tdm_driver *driver)
{
       /* A driver can register to only one adapter,
	* so no need to browse the list */
	mutex_lock(&core_lock);
	__detach_adapter(driver->adapter, driver);
	mutex_unlock(&core_lock);

	list_del(&driver->list);

	pr_debug("tdm-core: driver [%s] unregistered\n", driver->name);
}
EXPORT_SYMBOL(tdm_del_driver);

struct tdm_cmd_arg {
	unsigned cmd;
	void *arg;
};

static int tdm_cmd(struct tdm_driver *driver, void *_arg)
{
	struct tdm_cmd_arg *arg = _arg;

	if (driver->command)
		driver->command(arg->cmd, arg->arg);
	return 0;
}

/* this will send the given command for all the drivers of the adapters */
void tdm_driver_command(struct tdm_adapter *adap, unsigned int cmd, void *arg)
{
	struct tdm_cmd_arg cmd_arg;
	struct tdm_driver *driver, *next;

	cmd_arg.cmd = cmd;
	cmd_arg.arg = arg;

	/* Notify drivers */
	mutex_lock(&core_lock);
	list_for_each_entry_safe(driver, next, &driver_list, list) {
		if (tdm_device_match(driver, adap))
			tdm_cmd(driver, &cmd_arg);
	}
	mutex_unlock(&core_lock);
}
EXPORT_SYMBOL(tdm_driver_command);

static int __init tdm_init(void)
{
	pr_info("%s\n", __func__);
	return TDM_E_OK;
}

static void __exit tdm_exit(void)
{
	pr_info("%s\n", __func__);
	return;
}

/* We must initialize early, because some subsystems register tdm drivers
 * in subsys_initcall() code, but are linked (and initialized) before tdm.
 */
postcore_initcall(tdm_init);
module_exit(tdm_exit);

/* if read write debug required
#define TDM_CORE_DEBUG
*/

int tdm_read_direct(struct tdm_adapter *adap, u8 *buf, u32 len)
{
	int res;

	if (adap->algo->tdm_read_simple)
		res = adap->algo->tdm_read_simple(adap, buf, len);
	else {
		pr_err("TDM level read not supported\n");
		return -EOPNOTSUPP;
	}
	/* If everything went ok (i.e. frame received), return #bytes
	transmitted, else error code. */

	return res;


}
EXPORT_SYMBOL(tdm_read_direct);

int tdm_write_direct(struct tdm_adapter *adap, u8 *buf, u32 len)
{
	int res;

	if (adap->algo->tdm_write_simple)
		res = adap->algo->tdm_write_simple(adap, buf, len);
	else {
		pr_err("TDM level write not supported\n");
		return -EOPNOTSUPP;
	}

	return res;
}
EXPORT_SYMBOL(tdm_write_direct);




/* the functional interface to the tdm device. */
/*
 * tdm_master_send - issue a TDM write
 * @client: Handle to TDM device
 * @buf: Data that will be written to the TDM device
 * @count: How many bytes to write
 *
 * Returns negative errno, or else the number of bytes written.
 */
int tdm_master_send(struct tdm_adapter *adap, void **buf, int count)
{
	int res;

	if (adap->algo->tdm_write)
		res = adap->algo->tdm_write(adap, buf, count);
	else {
		pr_err("TDM level write not supported\n");
		return -EOPNOTSUPP;
	}

	/* If everything went ok (i.e. frame transmitted), return #bytes
	   transmitted, else error code. */
	return (res == 1) ? count : res;
}
EXPORT_SYMBOL(tdm_master_send);

/**
 * tdm_master_recv - issue a TDM read
 * @client: Handle to TDM device
 * @buf: Where to store data read from TDM device
 * @count: How many bytes to read
 *
 * Returns negative errno, or else the number of bytes read.
 */
int tdm_master_recv(struct tdm_adapter *adap, void **buf)
{
	int res;

	if (adap->algo->tdm_read)
		res = adap->algo->tdm_read(adap, (u16 **)buf);
	else {
		pr_err("TDM level read not supported\n");
		return -EOPNOTSUPP;
	}
	/* If everything went ok (i.e. frame received), return #bytes
	   transmitted, else error code. */
	return res;
}
EXPORT_SYMBOL(tdm_master_recv);

/**
 * tdm_master_recv - issue a TDM read
 * @client: Handle to TDM device
 * @buf: Where to store data read from TDM device
 * @count: How many bytes to read
 *
 * Returns negative errno, or else the number of bytes read.
 */
int tdm_master_get_write_buf(struct tdm_adapter *adap, void **buf)
{
	int res;

	if (adap->algo->tdm_get_write_buf) {
		res = adap->algo->tdm_get_write_buf(adap, (u16 **)buf);
	} else {
		pr_err("TDM level write buf get not supported\n");
		return -EOPNOTSUPP;
	}
	/* If everything went ok (i.e. 1 msg received), return #bytes
	   transmitted, else error code. */
	return res;
}
EXPORT_SYMBOL(tdm_master_get_write_buf);

int tdm_master_enable(struct tdm_driver *drv)
{
	int res;
	struct tdm_adapter *adap = drv->adapter;

	if (adap->algo->tdm_enable) {
		res = adap->algo->tdm_enable(adap);
	} else {
		pr_err("TDM level enable not supported\n");
		return -EOPNOTSUPP;
	}
	return res;
}
EXPORT_SYMBOL(tdm_master_enable);

int tdm_master_disable(struct tdm_driver *drv)
{
	int res;
	struct tdm_adapter *adap = drv->adapter;

	if (adap->algo->tdm_disable) {
		res = adap->algo->tdm_disable(adap);
	} else {
		pr_err("TDM level enable not supported\n");
		return -EOPNOTSUPP;
	}
	return res;
}
EXPORT_SYMBOL(tdm_master_disable);


struct tdm_adapter *tdm_get_adapter(int id)
{
	struct tdm_adapter *adapter;

	mutex_lock(&core_lock);
	adapter = idr_find(&tdm_adapter_idr, id);
	if (adapter && !try_module_get(adapter->owner))
		adapter = NULL;

	mutex_unlock(&core_lock);

	return adapter;
}
EXPORT_SYMBOL(tdm_get_adapter);

void tdm_put_adapter(struct tdm_adapter *adap)
{
	module_put(adap->owner);
}
EXPORT_SYMBOL(tdm_put_adapter);


unsigned int tdm_port_open(struct tdm_driver *driver, int chanid, void **h_port)
{
	struct tdm_port *port;
	struct tdm_adapter *adap;
	struct tdm_port_data	*p_port_data;
	unsigned long		flags;
	int res = TDM_E_OK;

	/* todo - verify that chanid is not already open */
	if (driver == NULL) {
		pr_err("driver NULL\n");
		return -ENODEV;
	}
	if (driver->adapter == NULL) {
		pr_err("adapter NULL\n");
		return -ENODEV;
	}

	adap = tdm_get_adapter(driver->adapter->id);
	if (!adap)
		return -ENODEV;

	/* This creates an anonymous tdm_port, which may later be
	 * pointed to some slot.
	 *
	 */
	port = kzalloc(sizeof(*port), GFP_KERNEL);
	if (!port) {
		res = -ENOMEM;
		goto out;
	}

	init_waitqueue_head(&port->ch_wait_queue);

	p_port_data = kzalloc(sizeof(struct tdm_port_data), GFP_KERNEL);
	if (!p_port_data) {
		res = -ENOMEM;
		goto outdata;
	}

	p_port_data->rx_data_fifo[TDM_CH_RX_BD_RING_SIZE-1].wrap = 1;
	p_port_data->tx_data_fifo[TDM_CH_TX_BD_RING_SIZE-1].wrap = 1;

	p_port_data->rx_in_data = p_port_data->rx_data_fifo;
	p_port_data->rx_out_data = p_port_data->rx_data_fifo;
	p_port_data->tx_in_data = p_port_data->tx_data_fifo;
	p_port_data->tx_out_data = p_port_data->tx_data_fifo;
	spin_lock_init(&p_port_data->rx_channel_lock);
	spin_lock_init(&p_port_data->tx_channel_lock);

	port->p_port_data = p_port_data;

	/* todo - these should be configured dynamically*/
	port->rx_max_frames = NUM_OF_FRAMES;
	port->ch_id = chanid;
	port->first_slot = chanid;

	port->slot_width = TDM_SLOT_WIDTH;

	/* todo - enable/disable the port with ioctl */
	port->in_use = 1;

	snprintf(driver->name, TDM_NAME_SIZE, "tdm-dev %d", chanid);
	port->driver = driver;

	spin_lock_irqsave(&adap->portlist_lock, flags);
	list_add_tail(&port->list, &adap->myports);
	spin_unlock_irqrestore(&adap->portlist_lock, flags);;

	*h_port = port;

out:
	return res;

outdata:
	unlock_kernel();
	kfree(port);
	return res;
}
EXPORT_SYMBOL(tdm_port_open);

unsigned int tdm_port_close(void *h_port)
{
	struct tdm_adapter *adap;
	struct tdm_driver *driver;
	struct tdm_port *port = (struct tdm_port *)h_port;
	unsigned long		flags;
	int ch_id, res = TDM_E_OK;

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle");
		return -ENXIO;
	}

	driver =  port->driver;
	ch_id = port->ch_id;

	if (driver == NULL) {
		pr_err("driver NULL\n");
		res = -ENODEV;
		goto out;
	}
	if (driver->adapter == NULL) {
		pr_err("adapter NULL\n");
		res = -ENODEV;
		goto out;
	}

	/* todo - verify that chanid is in open state */

	adap = driver->adapter;

	spin_lock_irqsave(&adap->portlist_lock, flags);
	list_del(&port->list);
	spin_unlock_irqrestore(&adap->portlist_lock, flags);

	if (port->p_port_data != NULL) {
		int i;
		struct tdm_bd *ch_bd;

		/* If the tdm is in channelised mode,
		de-allocate the channelised buffer */
		ch_bd = &(port->p_port_data->rx_data_fifo[0]);
		for (i = 0; ch_bd && i < TDM_CH_RX_BD_RING_SIZE; i++) {
			ch_bd->flag = 0;
			ch_bd++;
		}
		ch_bd = &(port->p_port_data->tx_data_fifo[0]);
		for (i = 0; ch_bd && i < TDM_CH_TX_BD_RING_SIZE; i++) {
			ch_bd->flag = 0;
			ch_bd++;
		}
		kfree(port->p_port_data);
	}
	kfree(port);
	port = NULL;
out:
	return res;
}
EXPORT_SYMBOL(tdm_port_close);

unsigned int tdm_port_ioctl(void *h_port, unsigned int cmd, unsigned long arg)
{
	struct tdm_port         *port = (struct tdm_port *)h_port;
	int                     port_num;
	int 			res = TDM_E_OK;

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle");
		return -ENXIO;
	}
	port_num =  port->ch_id;

	/* todo - verify that chanid is in open state */

	pr_info("TDM %d IOCTL cmd=0x%x arg=0x%x", port_num, cmd,
			(unsigned int)arg);

	switch (cmd) {
	case TDM_CHAN_ENABLE_TDM:
		pr_info("Port %d Enable TDM", port_num);
		port->in_use = 1;
		break;

	case TDM_CHAN_DISABLE_TDM:
		pr_info("Port %d Disable TDM", port_num);
		port->in_use = 0;
		break;

	case TDM_CHAN_SET_RX_LENGTH:
		pr_info("Set Receive Buffer Length for Portised Mode");
		if (port->in_use == 1) {
			pr_err("chan %d TDM Port is Enable", port_num);
			res = -EACCES;
			goto out_err;
		}
		if (arg%NUM_OF_FRAMES) {
			pr_info("Port %d Not a Multiple of %d", port_num,
					NUM_OF_FRAMES);
			res = -EINVAL;
				goto out_err;
		}
		/* todo - Not Supported port->rx_max_frames = arg;*/
		break;

	case TDM_CHAN_SET_START_SLOT:
		if (port->in_use == 1) {
			pr_err("chan %d TDM Port is Enable", port_num);
			res = -EACCES;
			goto out_err;
		}
		pr_info("Port %d set slot %d TDM", port_num, (int)arg);
		/* todo-currently not in use*/
		port->first_slot = arg;
		break;

	case TDM_CHAN_SET_SLOT_WIDTH:
		if (port->in_use == 1) {
			pr_err("chan %d TDM Port is Enable", port_num);
			res = -EACCES;
			goto out_err;
		}
		pr_info("Port %d set width %d TDM", port_num, (int)arg);
		/* todo- Not Supported port->slot_width = arg;*/
		break;

	default:
		pr_info("IOCTL Command Not Implemented");
		break;
	}

out_err:
	return res;
}
EXPORT_SYMBOL(tdm_port_ioctl);

unsigned int tdm_port_read(void *h_port, void *p_data, u16 *size)
{
	struct tdm_port *port = (struct tdm_port *)h_port;
	struct tdm_bd *rx_bd;
	unsigned long flags;
	int i, res = TDM_E_OK;
	unsigned short *buf, *buf1;

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle\n");
		return -ENXIO;
	}

	/* todo - verify that chanid is in open state  */
	if (!port->p_port_data || !port->in_use)
		return -EIO;

	spin_lock_irqsave(&port->p_port_data->rx_channel_lock, flags);
	rx_bd = port->p_port_data->rx_out_data;

	if (rx_bd->flag) {
		*size = rx_bd->length;
		buf = (u16 *) p_data;
		buf1 = (u16 *)rx_bd->p_data;
		for (i = 0; i < NUM_OF_FRAMES; i++)
			buf[i] = buf1[i];
		rx_bd->flag = 0;
		rx_bd->offset = 0;
		port->p_port_data->rx_out_data = (rx_bd->wrap) ?
				port->p_port_data->rx_data_fifo : rx_bd + 1;

	} else {
		spin_unlock_irqrestore(&port->p_port_data->rx_channel_lock,
						flags);
		pr_info("No Data Available");
		return -EAGAIN;
	}
	spin_unlock_irqrestore(&port->p_port_data->rx_channel_lock, flags);

	return res;
}
EXPORT_SYMBOL(tdm_port_read);


unsigned int tdm_port_write(void *h_port, void *p_data, u16 size)
{
	struct tdm_port *port = (struct tdm_port *)h_port;
	struct tdm_bd *tx_bd;
	unsigned long flags;
	int err = TDM_E_OK;
#ifdef TDM_CORE_DEBUG
	bool data_flag = 0;
#endif

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle");
		return -ENXIO;
	}

	if (p_data == NULL) { /* invalid data*/
		pr_err("Invalid Data");
		return -EFAULT;
	}

	/* todo - verify that chanid is in open state  */
	if (!port->p_port_data || !port->in_use)
		return -EIO;

	spin_lock_irqsave(&port->p_port_data->tx_channel_lock, flags);
	tx_bd = port->p_port_data->tx_in_data;

	if (!tx_bd->flag) {
		tx_bd->length = size;
		memcpy(tx_bd->p_data, p_data, size * port->slot_width);
		tx_bd->flag = 1;
		tx_bd->offset = 0;
		port->p_port_data->tx_in_data = (tx_bd->wrap) ?
				port->p_port_data->tx_data_fifo : tx_bd+1;
		port->port_stat.tx_pkt_count++;
#ifdef TDM_CORE_DEBUG
		data_flag = 1;
#endif
	} else {
		spin_unlock_irqrestore(&port->p_port_data->tx_channel_lock,
						flags);
		port->port_stat.tx_pkt_drop_count++;
		pr_err("E_NO_MEMORY -Failed Transmit");
		return -ENOMEM;
	}
	spin_unlock_irqrestore(&port->p_port_data->tx_channel_lock, flags);

#ifdef	TDM_CORE_DEBUG
	if (data_flag) {
		int k;
		pr_info("\nTX %d- Write - Port TX-%d\n", port->ch_id, size);
		for (k = 0; k < size; k++)
			pr_info("%x", p_data[k]);
		pr_info("\n");
	}
#endif
	return err;
}
EXPORT_SYMBOL(tdm_port_write);

wait_queue_head_t *tdm_port_get_wait_queue(void  *h_port)
{
	struct tdm_port *port = (struct tdm_port *)h_port;

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle");
		return NULL;
	}

	return &port->ch_wait_queue;

}
EXPORT_SYMBOL(tdm_port_get_wait_queue);

/* Driver Function for select and poll. Based on Port no, it sleeps on
 * waitque */
unsigned int tdm_port_poll(void *h_port, unsigned int wait_time)
{
	struct tdm_port *port = (struct tdm_port *)h_port;

	int rc;
	unsigned long timeout = msecs_to_jiffies(wait_time);

	if (port == NULL) { /* invalid handle*/
		pr_err("Invalid Handle\n");
		return -ENXIO;
	}
	/* todo - verify that chanid is in open state  */
	if (!port->p_port_data || !port->in_use)
		return -EIO;

	if (port->p_port_data->rx_out_data->flag) {
		pr_debug("Data Available");
		return TDM_E_OK;
	}
	if (timeout) {
		rc = wait_event_interruptible_timeout(port->ch_wait_queue,
					  port->p_port_data->rx_out_data->flag,
					  timeout);

		if (port->p_port_data->rx_out_data->flag) {
			pr_debug("Data Available");
			return TDM_E_OK;
		}
	}

	return -EAGAIN;
}
EXPORT_SYMBOL(tdm_port_poll);

unsigned int tdm_port_get_stats(void *h_port, struct tdm_port_stats *portStat)
{
	struct tdm_port *port = (struct tdm_port *)h_port;
	int port_num;

	if (port == NULL || portStat == NULL) { /* invalid handle*/
		pr_err("Invalid Handle");
		return -ENXIO;
	}
	port_num =  port->ch_id;

	memcpy(portStat, &port->port_stat, sizeof(struct tdm_port_stats));

	pr_info("TDM Port %d Get Stats", port_num);

	return TDM_E_OK;
}
EXPORT_SYMBOL(tdm_port_get_stats);

static int tdm_data_rx_deinterleave(struct tdm_adapter *adap)
{
	struct tdm_port *port, *next;
	struct tdm_bd	*ch_bd;

	int i, buf_size, ch_data_len = NUM_OF_FRAMES;
	/* todo - need to be generic for u8 and u16 etc */
	u16 *input_tdm_buffer;
	u16 *pcm_buffer;
	int framer_ch_data_size = NUM_OF_FRAMES;
	bool ch_data = 0;
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(TDM_ACTIVE_CHANNELS * TDM_SLOT_WIDTH, 8);
	int bytes_slot_offset = bytes_in_fifo_per_frame/TDM_SLOT_WIDTH;

	buf_size = tdm_master_recv(adap, (void **)&input_tdm_buffer);
	if (buf_size <= 0 || !input_tdm_buffer)
		return -EINVAL;

	/* de-interleaving for all ports*/
	list_for_each_entry_safe(port, next, &adap->myports, list) {
		/* if the port is not open */
		if (!port->in_use || !port->p_port_data)
			continue;

		ch_bd = port->p_port_data->rx_in_data;

		spin_lock(&port->p_port_data->rx_channel_lock);

		/*if old data is to be discarded */
		if (use_latest_tdm_data)
			if (ch_bd->flag) {
				ch_bd->flag = 0;
				ch_bd->offset = 0;
				if (ch_bd == port->p_port_data->rx_out_data)
					port->p_port_data->rx_out_data =
						ch_bd->wrap ?
						port->p_port_data->rx_data_fifo
						: ch_bd+1;
				port->port_stat.rx_pkt_drop_count++;
			}
		/* if the bd is empty */
		if (!ch_bd->flag) {
			if (ch_bd->offset == 0)
				ch_bd->length = port->rx_max_frames;

			pcm_buffer = ch_bd->p_data + ch_bd->offset;
			/* De-interleaving the data */
			for (i = 0; i < ch_data_len; i++) {
				pcm_buffer[i] =
					input_tdm_buffer[i*bytes_slot_offset +
								port->ch_id];
			}
			ch_bd->offset += ch_data_len * port->slot_width;

			if (ch_bd->offset >=
				(ch_bd->length - framer_ch_data_size)*
							port->slot_width) {
				ch_bd->flag = 1;
				ch_bd->offset = 0;
				port->p_port_data->rx_in_data = ch_bd->wrap ?
						port->p_port_data->rx_data_fifo
						: ch_bd+1;
				ch_data = 1;
			}
		} else {
			port->port_stat.rx_pkt_drop_count++;
		}

		spin_unlock(&port->p_port_data->rx_channel_lock);
		if (ch_data) {
			/*	Wake up the Port Data Poll event */
			wake_up_interruptible(&port->ch_wait_queue);
#ifdef	TDM_CORE_DEBUG
			pr_info("Port RX-%d-%d\n", port->ch_id, ch_data_len);
			for (i = 0; i < ch_data_len; i++)
				pr_info("%x", pcm_buffer[i]);
			pr_info("\n");
#endif
			port->port_stat.rx_pkt_count++;
			ch_data = 0;
		}
	}
	return TDM_E_OK;
}

static int tdm_data_tx_interleave(struct tdm_adapter *adap)
{
	struct tdm_port *port, *next;
	struct tdm_bd	*ch_bd;
	int i, buf_size, ch_data_len = NUM_OF_FRAMES;
	bool last_data = 0;
	/* todo - need to be generic for u8 and u16 etc */
	u16 *output_tdm_buffer;
	u16 *pcm_buffer;
	int framer_ch_data_size = NUM_OF_FRAMES;
	int bytes_in_fifo_per_frame =
	    ALIGN_SIZE(TDM_ACTIVE_CHANNELS * TDM_SLOT_WIDTH, 8);
	int bytes_slot_offset = bytes_in_fifo_per_frame/TDM_SLOT_WIDTH;

#ifdef TDM_CORE_DEBUG
	u8	data_flag = 0;
#endif

	buf_size = tdm_master_get_write_buf(adap, (void **)&output_tdm_buffer);
	if (buf_size <= 0 || !output_tdm_buffer)
		return -EINVAL;

	memset(output_tdm_buffer, 0, sizeof(buf_size));

	list_for_each_entry_safe(port, next, &adap->myports, list) {
		/* if the channel is open */
		if (!port->in_use || !port->p_port_data)
			continue;
		pr_debug("TX-Tdm %d (slots-)", port->ch_id);

		spin_lock(&port->p_port_data->tx_channel_lock);
		ch_bd = port->p_port_data->tx_out_data;
		if (ch_bd->flag) {
			pcm_buffer = (u16 *)((uint8_t *)ch_bd->p_data +
								ch_bd->offset);
			/*if the buffer has less frames than required */
			if (framer_ch_data_size >=
				((ch_bd->length) -
					(ch_bd->offset/port->slot_width))) {
				ch_data_len =
					(ch_bd->length) -
					(ch_bd->offset/port->slot_width);
				last_data = 1;
			} else {
				ch_data_len = framer_ch_data_size;
			}
			/* Interleaving the data */
			for (i = 0; i < ch_data_len; i++) {
				/* todo - need to be genric for any size
				   assignment*/
				output_tdm_buffer[port->ch_id +
					bytes_slot_offset * i] = pcm_buffer[i];
			}
			/* If all the data of this buffer is transmitted */
			if (last_data) {
				ch_bd->flag = 0;
				ch_bd->offset = 0;
				port->p_port_data->tx_out_data = ch_bd->wrap ?
						port->p_port_data->tx_data_fifo
						: ch_bd+1;
				port->port_stat.tx_pkt_conf_count++;
			} else {
				ch_bd->offset += ch_data_len*port->slot_width;
			}
#ifdef	TDM_CORE_DEBUG
		data_flag = 1;
#endif
		}
		spin_unlock(&port->p_port_data->tx_channel_lock);
	}

#ifdef	TDM_CORE_DEBUG
	if (data_flag) {
		pr_info("TX-TDM Interleaved Data-\n");
		for (i = 0; i < 64; i++)
			pr_info("%x", output_tdm_buffer[i]);
		pr_info("\n");
	  }
#endif
	return TDM_E_OK;
}

static int tdm_data_tx_full(struct tdm_adapter *adap)
{
	pr_err("TX full mode data fetch is not supported yet");
	return TDM_E_OK;
}

static int tdm_data_rx_full(struct tdm_adapter *adap)
{
	pr_err("RX full mode data fetch is not supported yet");
	return TDM_E_OK;
}

static void tdm_data_tasklet_fn(unsigned long data)
{
	struct tdm_adapter *adapter = (struct tdm_adapter *)data;
	if (adapter != NULL) {
		if (adapter->adap_mode & 0xF0) {
			tdm_data_tx_full(adapter);
			tdm_data_rx_full(adapter);
		} else {
			tdm_data_tx_interleave(adapter);
			tdm_data_rx_deinterleave(adapter);
		}
	}
}


MODULE_AUTHOR("Hemant Agrawal <hemant@freescale.com> and "
	"Rajesh Gumasta <rajesh.gumasta@freescale.com>");
MODULE_DESCRIPTION("TDM Driver Framework Core");
MODULE_LICENSE("GPL");
