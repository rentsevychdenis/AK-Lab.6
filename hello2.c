// SPDX-License-Identifier: GPL-2.0
/*
 * hello2: takes the "count" parameter and calls print_hello()
 * from hello1 that many times (AK-2, lab 6).
 *
 * Lab 6: an invalid count (> 10) triggers BUG_ON() instead of
 * printing an error and returning -EINVAL.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/bug.h>

#include "hello1.h"

MODULE_AUTHOR("Rentsevych D., IO-45");
MODULE_DESCRIPTION("Calls print_hello() from hello1 'count' times, BUG_ON() for count > 10");
MODULE_LICENSE("GPL");

#define HELLO_MAX_COUNT	10
#define HELLO_WARN_MIN	5

static uint count = 1;
module_param(count, uint, 0444);
MODULE_PARM_DESC(count,
		 "Number of 'Hello, world!' lines (default 1; 0 or 5..10 warn; >10 is BUG())");

static int __init hello2_init(void)
{
	uint i;
	int ret;

	BUG_ON(count > HELLO_MAX_COUNT);

	if (count == 0 || count >= HELLO_WARN_MIN)
		pr_warn("count=%u is 0 or between %d and %d, continuing\n",
			count, HELLO_WARN_MIN, HELLO_MAX_COUNT);

	for (i = 0; i < count; i++) {
		ret = print_hello();
		if (ret)
			return ret;
	}

	return 0;
}

static void __exit hello2_exit(void)
{
	pr_info("unloaded\n");
}

module_init(hello2_init);
module_exit(hello2_exit);
