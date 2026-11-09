// SPDX-License-Identifier: GPL-2.0
/*
 * hello1: exports print_hello() and keeps the list of print events
 * with the time spent on every print (AK-2, lab 6).
 *
 * Lab 6: fault injection - the fail_at-th call of print_hello()
 * behaves as if kmalloc() returned 0, and the NULL pointer is
 * dereferenced on purpose to get a kernel oops.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#include "hello1.h"

MODULE_AUTHOR("Rentsevych D., IO-45");
MODULE_DESCRIPTION("Exports print_hello(), measures every print, injects a kmalloc() fault");
MODULE_LICENSE("GPL");

static uint fail_at = 5;
module_param(fail_at, uint, 0444);
MODULE_PARM_DESC(fail_at,
		 "Number of the print_hello() call that gets NULL (default 5, 0 = never)");

static LIST_HEAD(hello_events);
static uint hello_calls;

int print_hello(void)
{
	struct hello_event *ev;

	hello_calls++;

	/* Fault injection: pretend that kmalloc() returned 0 */
	if (hello_calls == fail_at)
		ev = NULL;
	else
		ev = kmalloc_obj(*ev, GFP_KERNEL);

	/*
	 * No NULL check on purpose: the fail_at-th call writes to
	 * ev->start at address 0 + offsetof(struct hello_event, start)
	 * and the kernel reports an oops.
	 */
	ev->start = ktime_get();
	pr_info("Hello, world!\n");
	ev->end = ktime_get();

	list_add_tail(&ev->list, &hello_events);

	return 0;
}
EXPORT_SYMBOL(print_hello);

static int __init hello1_init(void)
{
	pr_info("loaded, fail_at=%u\n", fail_at);
	return 0;
}

static void __exit hello1_exit(void)
{
	struct hello_event *ev, *tmp;
	unsigned int i = 0;

	list_for_each_entry_safe(ev, tmp, &hello_events, list) {
		pr_info("print %u: start %lld ns, took %lld ns\n", ++i,
			ktime_to_ns(ev->start),
			ktime_to_ns(ktime_sub(ev->end, ev->start)));
		list_del(&ev->list);
		kfree(ev);
	}

	pr_info("unloaded, %u print(s) recorded\n", i);
}

module_init(hello1_init);
module_exit(hello1_exit);
