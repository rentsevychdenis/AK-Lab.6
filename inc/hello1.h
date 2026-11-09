/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shared interface of the hello1 and hello2 modules (AK-2, lab 6).
 */

#ifndef HELLO1_H
#define HELLO1_H

#include <linux/list.h>
#include <linux/ktime.h>

/**
 * struct hello_event - one call of print_hello()
 * @list:  list node, the list head lives in hello1
 * @start: time right before the greeting is printed
 * @end:   time right after the greeting is printed
 */
struct hello_event {
	struct list_head list;
	ktime_t start;
	ktime_t end;
};

/**
 * print_hello() - print "Hello, world!" and remember how long it took
 *
 * Exported by hello1. Every call adds a struct hello_event to the
 * list kept by hello1; the list is printed and freed when hello1
 * is unloaded.
 *
 * Lab 6: the fail_at-th call (parameter of hello1) pretends that
 * kmalloc() returned NULL, which causes a NULL pointer dereference
 * (kernel oops) on purpose.
 *
 * Return: 0 on success.
 */
int print_hello(void);

#endif /* HELLO1_H */
