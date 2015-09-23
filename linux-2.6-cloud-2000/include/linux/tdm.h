/* include/linux/tdm.h
 *
 * Copyright (C) 2010-2011 Freescale Semiconductor, Inc, All rights reserved.
 *
 * tdm.h - definitions for the tdm-device framework interface
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


#ifndef _LINUX_TDM_H
#define _LINUX_TDM_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>	/* for struct device */
#include <linux/sched.h>	/* for completion */
#include <linux/mutex.h>
#include <linux/interrupt.h>

/* channel coading RCS/TCS field of RFP/TFP */
#define CHANNEL_8BIT_LIN	0x00000000	/* 8 bit linear */
#define CHANNEL_8BIT_ULAW	0x00000001	/* 8 bit Mu-law */
#define CHANNEL_8BIT_ALAW	0x00000002	/* 8 bit A-law */
#define CHANNEL_16BIT_LIN	0x00000003	/* 16 bit Linear */

/*todo - These parameters needs to be configured dynamically*/
#define TDM_ACTIVE_CHANNELS 	16	/* Number of channels active */
#define TDM_CHANNEL_TYPE 	CHANNEL_16BIT_LIN
#if (TDM_CHANNEL_TYPE == CHANNEL_16BIT_LIN)
	#define TDM_SLOT_WIDTH 2
#else
	#define TDM_SLOT_WIDTH 1
#endif

#define CH_1_MS_FRAMES		8
#define NUM_MS			10
#define NUM_OF_FRAMES		(NUM_MS * CH_1_MS_FRAMES) /* number of frames
							   for 1 buffer */
/* Port data size for one Processing Cycle */
#define CH_DATA_SIZE		(NUM_OF_FRAMES * TDM_SLOT_WIDTH)

/* General options */

struct tdm_algorithm;
struct tdm_adapter;
struct tdm_port;
struct tdm_driver;

/* Align addr on a size boundary - adjust address up if needed */
static inline int ALIGN_SIZE(u32 size, u32 alignment)
{
	return (size + alignment - 1) & (~(alignment - 1));
}

#if defined(CONFIG_TDM) || defined(CONFIG_TDM_MODULE)
extern int tdm_master_enable(struct tdm_driver *);
extern int tdm_master_disable(struct tdm_driver *);
int tdm_master_send(struct tdm_adapter *adap, void **buf, int count);
int tdm_master_recv(struct tdm_adapter *adap, void **buf);
int tdm_read_direct(struct tdm_adapter *adap, u8 *buf, u32 len);
int tdm_write_direct(struct tdm_adapter *adap, u8 *buf, u32 len);

#endif /* TDM */

/**
 * struct tdm_driver - represent an TDM device driver
 * @class: What kind of tdm device we instantiate (for detect)
 * @id:Driver id
 * @name: Name of the driver
 * @attach_adapter: Callback for device addition (for legacy drivers)
 * @detach_adapter: Callback for device removal (for legacy drivers)
 * @probe: Callback for device binding
 * @remove: Callback for device unbinding
 * @shutdown: Callback for device shutdown
 * @suspend: Callback for device suspend
 * @resume: Callback for device resume
 * @command: Callback for sending commands to device
 * @id_table: List of TDM devices supported by this driver
 * @list: List of drivers created (for tdm-core use only)
 */
struct tdm_driver {
	unsigned int class;
	unsigned int id;
	char name[TDM_NAME_SIZE];

	int (*attach_adapter)(struct tdm_adapter *);
	int (*detach_adapter)(struct tdm_adapter *);

	/* Standard driver model interfaces */
	int (*probe)(const struct tdm_device_id *);
	int (*remove)(void);

	/* driver model interfaces that don't relate to enumeration */
	void (*shutdown)(void);
	int (*suspend)(pm_message_t mesg);
	int (*resume)(void);

	/* a ioctl like command that can be used to perform specific functions
	 * with the device.
	 */
	int (*command)(unsigned int cmd, void *arg);

	const struct tdm_device_id *id_table;

	/* The associated adapter for this driver */
	struct tdm_adapter *adapter;
	struct list_head list;
};

/* tdm per port statistics structure, used for providing and storing tdm port
 * statistics.
 */
struct tdm_port_stats{
	unsigned int rx_pkt_count;	/* Rx frame count per channel */
	unsigned int rx_pkt_drop_count;	/* Rx drop count per channel to
					   clean space for new buffer */
	unsigned int tx_pkt_count;	/* Tx frame count per channel */
	unsigned int tx_pkt_conf_count;	/* Tx frame confirmation count per
					   channel */
	unsigned int tx_pkt_drop_count;	/* Tx drop count per channel due to
					   queue full */
};


/* tdm Buffer Descriptor, used for Creating Interleaved and De-interleaved
 * FIFOs
 */
struct tdm_bd {
   unsigned char flag;		/* BD is full or empty */
   unsigned char wrap;		/* BD is last in the queue */
   unsigned short length;	/* Length of Data in BD */
   /*todo use dyanmic memory
   unsigned long *p_data; */	/* Data Pointer */
   unsigned short p_data[NUM_OF_FRAMES];	/* Data Pointer */
   unsigned long offset;	/* Offset of the Data Pointer to be used */
};

#define TDM_CH_RX_BD_RING_SIZE  3
#define TDM_CH_TX_BD_RING_SIZE  3

/* tdm RX-TX Channelised Data */
struct tdm_port_data {
	struct tdm_bd rx_data_fifo[TDM_CH_RX_BD_RING_SIZE]; /* Rx Port Data BD
								Ring */
	struct tdm_bd *rx_in_data;	/* Current Port Rx BD to be filled by
						de-interleave function */
	struct tdm_bd *rx_out_data;  /* Current Port Rx BD to be read by App */
	struct tdm_bd tx_data_fifo[TDM_CH_TX_BD_RING_SIZE]; /* Tx Port Data BD
								Ring */
	struct tdm_bd *tx_in_data;	/* Current Port Tx BD to be filled by
						App */
	struct tdm_bd *tx_out_data;	/* Current Port Tx BD to be read by
						interleave function */
	spinlock_t rx_channel_lock;	/* Spin Lock for Rx Port */
	spinlock_t tx_channel_lock;	/* Spin Lock for Tx Port */
};

/* struct tdm_port - represent an TDM ports for a device */
struct tdm_port {
	unsigned short flags;	/* div., see below */
	unsigned short ch_id;	/* port ID - At present it is same as slot
					Number */
	unsigned short  first_slot;	/* Not Used */
	unsigned short slot_width;	/* slot width for this port */
	unsigned short in_use;		/* Port is enabled? */
	uint16_t rx_max_frames; 	/* Received Port frames
					   before allowing Read Operation in
					   Port Mode */

	struct tdm_port_stats port_stat;/* A structure parameters defining
					   TDM port statistics. */
	struct tdm_port_data *p_port_data;	/* a structure parameters
						defining tdm channelised data */
	wait_queue_head_t ch_wait_queue;	/* waitQueue for RX Port Data */

	struct tdm_driver *driver;	/* driver for this port */
	struct list_head list;		/* list of ports */
};

/* tdm_algorithm is for accessing the routines of device */
struct tdm_algorithm {
	u32 (*tdm_read)(struct tdm_adapter *, u16 **);
	u32 (*tdm_get_write_buf)(struct tdm_adapter *, u16 **);
	u32 (*tdm_write)(struct tdm_adapter *, void * , unsigned int len);
	int (*tdm_enable)(struct tdm_adapter *);
	int (*tdm_disable)(struct tdm_adapter *);
	int (*tdm_read_simple)(struct tdm_adapter *, u8 *, u32 len);
	int (*tdm_write_simple)(struct tdm_adapter *, u8 *, u32 len);
	/* To determine what the adapter supports */
	u32 (*functionality) (struct tdm_adapter *);
};

/* tdm_adapter_mode is to define in mode of the device */
enum tdm_adapter_mode {
	e_TDM_ADAPTER_MODE_NONE = 0x00,
	e_TDM_ADAPTER_MODE_T1 = 0x01,
	e_TDM_ADAPTER_MODE_E1 = 0x02,
	e_TDM_ADAPTER_MODE_T1_RAW = 0x10,
	e_TDM_ADAPTER_MODE_E1_RAW = 0x20,
};

/* tdm_process_mode used for testing the tdm device in normal mode or internal
 * loopback or external loopback
 */
enum tdm_process_mode{
	e_TDM_PROCESS_NORMAL = 0	/* Normal mode */
	, e_TDM_PROCESS_INT_LPB = 1	/* Internal loop mode */
	, e_TDM_PROCESS_EXT_LPB = 2	/* External Loopback mode */
};

/*
 * tdm_adapter is the structure used to identify a physical tdm device along
 * with the access algorithms necessary to access it.
 */
struct tdm_adapter {
	struct module *owner;	/* owner of the adapter module */
	unsigned int id;	/* Adapter Id */
	unsigned int class;	/* classes to allow probing for */
	unsigned int drv_count;	/* Number of drivers associated with the
				   adapter */

	const struct tdm_algorithm *algo;	/* the algorithm to access the
						   adapter*/

	char name[TDM_NAME_SIZE];	/* Name of Adapter */
	int tdm_mode;	/* Not Used : loopback or normal */
	int adap_mode;	/* 0=None, 1= T1, 2= T1-FULL, 3=E1, 4 = E1-FULL */
	int max_num_ports;	/* Not Used: Max Number of ports that can be
				   created on this adapter */
	struct mutex adap_lock;
	struct device *parent;	/*Not Used*/

	struct tasklet_struct tdm_data_tasklet;	/* tasklet handle to perform
						   data processing*/
	int tasklet_conf;	/* flag for tasklet configuration */
	int tdm_rx_flag;

	struct list_head myports;	/* list of ports, created on this
					   adapter */
	struct list_head list;
	spinlock_t   portlist_lock;   /* Spin Lock for port_list */
	void *data;
};

static inline void *tdm_get_adapdata(const struct tdm_adapter *dev)
{
	return dev->data;
}

static inline void tdm_set_adapdata(struct tdm_adapter *dev, void *data)
{
       dev->data = data;
}

/* functions exported by tdm.o */

#if defined(CONFIG_TDM) || defined(CONFIG_TDM_MODULE)
extern int tdm_add_adapter(struct tdm_adapter *);
extern int tdm_del_adapter(struct tdm_adapter *);
extern int tdm_register_driver(struct module *, struct tdm_driver *);
extern void tdm_del_driver(struct tdm_driver *);

extern unsigned int tdm_port_open(struct tdm_driver *, int , void **);
extern unsigned int tdm_port_close(void *);
extern unsigned int tdm_port_ioctl(void *, unsigned int, unsigned long);
extern unsigned int tdm_port_read(void *, void *, u16 *);
extern unsigned int tdm_port_write(void *, void *, u16);
extern unsigned int tdm_port_poll(void *, unsigned int);

static inline int tdm_add_driver(struct tdm_driver *driver)
{
	return tdm_register_driver(THIS_MODULE, driver);
}

/* call the tdm_driver->command() of all attached drivers with the given
 * arguments */
extern void tdm_driver_command(struct tdm_adapter *, unsigned int, void *);
extern struct tdm_adapter *tdm_get_adapter(int id);
extern void tdm_put_adapter(struct tdm_adapter *adap);

/* Return the functionality mask */
static inline u32 tdm_get_functionality(struct tdm_adapter *adap)
{
	return adap->algo->functionality(adap);
}

/* Return 1 if adapter supports everything we need, 0 if not. */
static inline int tdm_check_functionality(struct tdm_adapter *adap, u32 func)
{
	return (func & tdm_get_functionality(adap)) == func;
}

#endif /* TDM */
#endif /* __KERNEL__ */

#define TDM_E_OK 0

typedef enum {
	TDM_CHAN_SET_MODE = 0,	/* Set the channel Mode (Full/Channelized) */
	TDM_CHAN_SET_START_SLOT,/*Starting slot fo the port */
	TDM_CHAN_SET_SLOT_WIDTH,/* Width of the slot */
	TDM_CHAN_ENABLE_TDM,	/* Enable the TDM port of respective channel */
	TDM_CHAN_DISABLE_TDM,	/* Disable the TDM port of respective channel */
	TDM_CHAN_SET_RX_LENGTH	/* Minimum Received Port Buffer Length
				   before allowing Read Operation in Port Mode*/
} tdm_cmd_types;

#endif /* _LINUX_TDM_H */
