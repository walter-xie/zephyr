/*
 * Copyright (c) 2017 CPqD Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <misc/printk.h>
#include <stdlib.h>
#include <rpl.h>
#include <watchdog.h>
#include <uart.h>

#define CURRENT_VERSION 0

#define WATCHDOG_DEV_NAME "WDT"


static void create_root_dag(void)
{
    struct net_if *iface = net_if_get_default();
	if (!iface) {
		printk("Interface is NULL\n");
		printk("Failed to setup RPL root node\n");
		return;
	}

	u8_t init_version = CURRENT_VERSION;
	/* Check the current version */
	#if (CURRENT_VERSION == 0) 
	/* case 0 - call init */
	init_version = net_rpl_lollipop_init();
	#elif (CURRENT_VERSION > 0) 
	/* case > 0 - increment */
	net_rpl_lollipop_increment(&init_version);
	#endif
	
	struct in6_addr ipv6_addr;
	net_ipv6_addr_create_iid(&ipv6_addr ,net_if_get_link_addr(iface));
	/* Setup the root node */
    printk("RPL border router starting\n");
	struct net_rpl_dag *dag = net_rpl_set_root_with_version(iface, CONFIG_NET_RPL_DEFAULT_INSTANCE, &ipv6_addr, init_version);
	if (!dag) {
		printk("Cannot set root node");
		return;
	}
	
	if (!net_rpl_set_prefix(iface, dag, &ipv6_addr, 16)) {
		printk("Cannot set prefix");
	}
}

static struct device *wdg_dev=(struct device *)0;
void main(void)
{
	wdg_dev = device_get_binding(WATCHDOG_DEV_NAME);
	wdt_reload(wdg_dev);
	create_root_dag();
    while(1)
	{
		wdt_reload(wdg_dev);
		k_sleep(300);
	}
}
