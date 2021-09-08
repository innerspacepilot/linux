/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kmsg_dump.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/tick.h>
#include <linux/file.h>
#include <linux/sec_debug.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/mount.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include <asm/cacheflush.h>
#include <asm/stacktrace.h>

#ifdef CONFIG_SEC_DEBUG

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
/*union sec_debug_level_t {
	u32 uint_val;
} sec_debug_level;

static int sec_debug_reserve_ok;

int sec_debug_get_debug_level(void)
{
	return sec_debug_level.uint_val;
}
*/
/* layout of SDRAM : First 4KB of DRAM
*         0x0: magic            (4B)
*   0x4~0x3FF: panic string     (1020B)
* 0x400~0x7FF: panic Extra Info (1KB)
* 0x800~0xFFB: panic dumper log (2KB - 4B)
*       0xFFC: copy of magic    (4B)
*/

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
static char *last_kmsg_buffer;
static size_t last_kmsg_size;
void sec_debug_save_last_kmsg(unsigned char *head_ptr, unsigned char *curr_ptr, size_t buf_size)
{
	size_t size;
	unsigned char *magickey_addr;

	if (!head_ptr || !curr_ptr || head_ptr == curr_ptr) {
		pr_err("%s: no data\n", __func__);
		return;
	}

	if ((curr_ptr - head_ptr) <= 0) {
		pr_err("%s: invalid args\n", __func__);
		return;
	}
	size = (size_t)(curr_ptr - head_ptr);

	magickey_addr = head_ptr + buf_size - (size_t)0x08;

	/* provide previous log as last_kmsg */
	if (*((unsigned long long *)magickey_addr) == SEC_LKMSG_MAGICKEY) {
		pr_info("%s: sec_log buffer is full\n", __func__);
		last_kmsg_size = (size_t)SZ_2M;
		last_kmsg_buffer = kzalloc(last_kmsg_size, GFP_NOWAIT);
		if (last_kmsg_size && last_kmsg_buffer) {
			memcpy(last_kmsg_buffer, curr_ptr, last_kmsg_size - size);
			memcpy(last_kmsg_buffer + (last_kmsg_size - size), head_ptr, size);
			pr_info("%s: successed\n", __func__);
		} else {
			pr_err("%s: failed\n", __func__);
		}
	} else {
		pr_info("%s: sec_log buffer is not full\n", __func__);
		last_kmsg_size = size;
		last_kmsg_buffer = kzalloc(last_kmsg_size, GFP_NOWAIT);
		if (last_kmsg_size && last_kmsg_buffer) {
			memcpy(last_kmsg_buffer, head_ptr, last_kmsg_size);
			pr_info("%s: successed\n", __func__);
		} else {
			pr_err("%s: failed\n", __func__);
		}
	}
}

static ssize_t sec_last_kmsg_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t)(last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_last_kmsg_read,
};

static int __init sec_last_kmsg_late_init(void)
{
	struct proc_dir_entry *entry;

	if (!last_kmsg_buffer)
		return 0;

	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO,
			    NULL, &last_kmsg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, last_kmsg_size);
	return 0;
}

late_initcall(sec_last_kmsg_late_init);
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

#endif /* CONFIG_SEC_DEBUG */

