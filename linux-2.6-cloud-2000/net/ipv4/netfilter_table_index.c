/*
 * This is a module which is used for L3 Firewall acceleration
 *
 * This module adds a table for index. When a new IP packet comes,
 * it will go along the original filter path, then a message about
 * the IP packet can be sent to the table so that the table can
 * remember the IP packet as an acquaintance, and the action in
 * the original filter path will be recorded either.
 * When another IP packet comes, it will be searched in the table
 * and then it will be judged whether it is an acquainted IP packet
 * or not. If it is found, in another word, if it is an acquainted
 * IP packet, it will take the last action recorded in the table,
 * but it does not need to go along the old long distance, this is
 * the shortest path.
 * In this way, we shortened the IP packet filter path to increase
 * the Firewall performance.
 *
 * Copyright (C) 2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Authors: Jianhua Xie(Adam) <jianhua.xie@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netfilter_ipv4.h>
#include <net/checksum.h>
#include <linux/route.h>
#include <net/route.h>
#include <net/xfrm.h>

#include <linux/netfilter_table_index.h>
#include <linux/netfilter/x_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianhua Xie <b29408@freescale.com>");
MODULE_DESCRIPTION("L3 Firewall accelorator");

#ifdef CONFIG_NETFILTER_TABLE_INDEX

struct NF_TABLE_INDEX *p_netfilter_table_index;
EXPORT_SYMBOL_GPL(p_netfilter_table_index);


int init_netfilter_table_index()
{
	memset(p_netfilter_table_index->table_index,
		0, sizeof(struct TABLE_INDEX) * MAXLINES_OF_TABLE);
	return 0;
}
EXPORT_SYMBOL_GPL(init_netfilter_table_index);

/* Record a new ip packet and the netfilter's response at the
 * top line of the acceleration table, including the destination
 * ip of the packet, the net device which the packet comes from,
 * and the netfilter's response.
 * The function returns 0 forever.
 */
int update_netfilter_table_index(
	u_int8_t pf, /* NFPROTO_IPV4 */
	unsigned int hook, /* NF_INET_FORWARD */
	struct sk_buff *skb,
	struct net_device *indev,
	struct net_device *outdev,
	struct NF_TABLE_INDEX *p_netfilter_table_index,
	unsigned int response)
{
	struct TABLE_INDEX *p = (struct TABLE_INDEX *)
		&(p_netfilter_table_index->table_index[0]);
	/* we only cache IPV4 and Forwarding */
	if (NFPROTO_IPV4 != pf)
		return 0;
	if (NF_INET_FORWARD != hook)
		return 0;
	/* we only cache TCP/UDP protocol, others go the slow path */
	if ((IPPROTO_TCP != ip_hdr(skb)->protocol) &&
		(IPPROTO_UDP != ip_hdr(skb)->protocol))
			return 0;
	/* we only cache NF_STOP/NF_DROP/NF_ACCEPT actions */
	if ((NF_DROP != response) && (NF_STOP != response) &&
		(NF_ACCEPT != response))
			return 0;
	write_lock_bh(&(p_netfilter_table_index->lock));
	memmove((void *)&(p_netfilter_table_index->table_index[1]),
		(void *)&(p_netfilter_table_index->table_index[0]),
		(MAXLINES_OF_TABLE - 1)*sizeof(struct TABLE_INDEX));

	if (IPPROTO_TCP == ip_hdr(skb)->protocol) {
		p[0].Saddr	= ip_hdr(skb)->saddr;
		p[0].Daddr	= ip_hdr(skb)->daddr;
		p[0].Protocol	= ip_hdr(skb)->protocol;
		p[0].SourcePort	= tcp_hdr(skb)->source;
		p[0].DestPort	= tcp_hdr(skb)->dest;
		p[0].response	= response;
	} else if (IPPROTO_UDP == ip_hdr(skb)->protocol) {
		p[0].Saddr	= ip_hdr(skb)->saddr;
		p[0].Daddr	= ip_hdr(skb)->daddr;
		p[0].Protocol	= ip_hdr(skb)->protocol;
		p[0].SourcePort	= udp_hdr(skb)->source;
		p[0].DestPort	= udp_hdr(skb)->dest;
		p[0].response	= response;
	}
	write_unlock_bh(&(p_netfilter_table_index->lock));
	return 0;
}
EXPORT_SYMBOL_GPL(update_netfilter_table_index);

/* Search a new ip packet from the acceleration table, if it is found,
 * the function returns the response in the same line of the table,
 * else it returns -1.
 */
int match_netfilter_table_index(
		u_int8_t pf, /* NFPROTO_IPV4 */
		unsigned int hook, /* NF_INET_FORWARD */
		struct sk_buff *skb,
		struct net_device *indev,
		struct net_device *outdev,
		struct NF_TABLE_INDEX *p_netfilter_table_index)
{
	bool found = false;
	unsigned long i;
	unsigned int response = -1;
	struct TABLE_INDEX *p = (struct TABLE_INDEX *)
		&(p_netfilter_table_index->table_index[0]);
	/* we only cache IPV4 and Forwarding */
	if (NFPROTO_IPV4 != pf)
		return -1;
	if (NF_INET_FORWARD != hook)
		return -1;
	/* we only search TCP/UDP protocol, others go the slow path */
	if ((IPPROTO_TCP != ip_hdr(skb)->protocol) &&
		(IPPROTO_UDP != ip_hdr(skb)->protocol))
		return -1;
	if (IPPROTO_TCP == ip_hdr(skb)->protocol) {
		read_lock_bh(&(p_netfilter_table_index->lock));
		for (i = 0; i < MAXLINES_OF_TABLE; i++) {
			if ((p[i].Saddr == ip_hdr(skb)->saddr) &&
				(p[i].Daddr == ip_hdr(skb)->daddr) &&
				(p[i].Protocol == ip_hdr(skb)->protocol) &&
				(p[i].SourcePort == tcp_hdr(skb)->source) &&
				(p[i].DestPort == tcp_hdr(skb)->dest)) {

				response = p[i].response;
				read_unlock_bh(
					&(p_netfilter_table_index->lock));
				found = true;
				goto out;
				}
		}
	read_unlock_bh(&(p_netfilter_table_index->lock));
	} else if (IPPROTO_UDP == ip_hdr(skb)->protocol) {
		read_lock_bh(&(p_netfilter_table_index->lock));
		for (i = 0; i < MAXLINES_OF_TABLE; i++) {
			if ((p[i].Saddr == ip_hdr(skb)->saddr) &&
				(p[i].Daddr == ip_hdr(skb)->daddr) &&
				(p[i].Protocol == ip_hdr(skb)->protocol) &&
				(p[i].SourcePort == udp_hdr(skb)->source) &&
				(p[i].DestPort == udp_hdr(skb)->dest)) {

				response = p[i].response;
				read_unlock_bh(
					&(p_netfilter_table_index->lock));
				found = true;
				goto out;
				}
		}
		read_unlock_bh(&(p_netfilter_table_index->lock));
	}
out:
	/* if not found, the response is -1 just like the initial value */
	return response;
}
EXPORT_SYMBOL_GPL(match_netfilter_table_index);

static int __init netfilter_table_index_init_early(void)
{

	p_netfilter_table_index = (struct NF_TABLE_INDEX *)
		kmalloc(sizeof(struct NF_TABLE_INDEX), GFP_KERNEL);
	if (!p_netfilter_table_index)
		return -ENOMEM;
	if (init_netfilter_table_index()) {
		kfree(p_netfilter_table_index);
		printk(KERN_INFO "%s:not enough memory.\n", __func__);
		return -ENOMEM;
	}
	rwlock_init(&(p_netfilter_table_index->lock));
	printk(KERN_INFO "%s:init ok\nMAXLINES_OF_TABLE:%d\n",
			__func__, MAXLINES_OF_TABLE);
	return 0;
}

static void __exit netfilter_table_index_exit(void)
{
	kfree(p_netfilter_table_index);
	printk(KERN_INFO "%s exit ok\n.", __func__);
}

module_init(netfilter_table_index_init_early);
module_exit(netfilter_table_index_exit);

#endif /* end CONFIG_NETFILTER_TABLE_INDEX */
