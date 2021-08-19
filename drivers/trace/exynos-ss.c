/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/ktime.h>
#include <linux/printk.h>
#include <linux/exynos-ss.h>
#include <soc/samsung/exynos-condbg.h>
#include <linux/kallsyms.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>
#include <linux/input.h>
#include <linux/of_address.h>
#include <linux/ptrace.h>
#include <linux/exynos-sdm.h>
#include <linux/exynos-ss-soc.h>
#include <linux/clk-provider.h>

#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/memory.h>
#include <asm/mmu.h>
#include <asm/smp_plat.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/acpm_ipc_ctrl.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <soc/samsung/exynos-modem-ctrl.h>

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif /* CONFIG_SEC_DEBUG */

#include <linux/nmi.h>

/*  Size domain */
#define ESS_KEEP_HEADER_SZ		(SZ_256 * 3)
#define ESS_HEADER_SZ			SZ_4K
#define ESS_MMU_REG_SZ			SZ_4K
#define ESS_CORE_REG_SZ			SZ_4K
#define ESS_SPARE_SZ			SZ_16K
#define ESS_HEADER_TOTAL_SZ		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ + ESS_SPARE_SZ)
#define ESS_HEADER_ALLOC_SZ		SZ_2M

/*  Length domain */
#define ESS_LOG_STRING_LENGTH		SZ_128
#define ESS_MMU_REG_OFFSET		SZ_512
#define ESS_CORE_REG_OFFSET		SZ_512
#define ESS_CORE_PC_OFFSET		0x600
#define ESS_LOG_MAX_NUM			SZ_1K
#define ESS_API_MAX_NUM			SZ_2K
#define ESS_EX_MAX_NUM			SZ_8
#define ESS_IN_MAX_NUM			SZ_8
#define ESS_CALLSTACK_MAX_NUM		CONFIG_EXYNOS_SNAPSHOT_CALLSTACK
#define ESS_ITERATION			5
#define ESS_NR_CPUS			NR_CPUS
#define ESS_ITEM_MAX_NUM		10

/* Sign domain */
#define ESS_SIGN_RESET			0x0
#define ESS_SIGN_RESERVED		0x1
#define ESS_SIGN_SCRATCH		0xD
#define ESS_SIGN_ALIVE			0xFACE
#define ESS_SIGN_DEAD			0xDEAD
#define ESS_SIGN_PANIC			0xBABA
#define ESS_SIGN_SAFE_FAULT		0xFAFA
#define ESS_SIGN_NORMAL_REBOOT		0xCAFE
#define ESS_SIGN_FORCE_REBOOT		0xDAFE

/*  Specific Address Information */
#define ESS_FIXED_VIRT_BASE		(VMALLOC_START + 0xEE000000)
#define ESS_OFFSET_SCRATCH		(0x100)
#define ESS_OFFSET_LAST_LOGBUF		(0x200)
#define ESS_OFFSET_EMERGENCY_REASON	(0x300)
#define ESS_OFFSET_CORE_POWER_STAT	(0x400)
#define ESS_OFFSET_PANIC_STAT		(0x500)
#define ESS_OFFSET_LAST_PC		(0x600)

/* S5P_VA_SS_BASE + 0xC00 -- 0xFFF is reserved */
#define ESS_OFFSET_PANIC_STRING 	(0xC00)
#define ESS_OFFSET_SPARE_BASE		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)

#define mpidr_cpu_num(mpidr)			\
	( MPIDR_AFFINITY_LEVEL(mpidr, 1) << 2	\
	 | MPIDR_AFFINITY_LEVEL(mpidr, 0))

struct exynos_ss_base {
	size_t size;
	size_t vaddr;
	size_t paddr;
	unsigned int persist;
	unsigned int enabled;
	unsigned int enabled_init;
};

struct exynos_ss_item {
	char *name;
	struct exynos_ss_base entry;
	unsigned char *head_ptr;
	unsigned char *curr_ptr;
	unsigned long long time;
};

struct exynos_ss_log {
	struct task_log {
		unsigned long long time;
		unsigned long sp;
		struct task_struct *task;
		char task_comm[TASK_COMM_LEN];
	} task[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct work_log {
		unsigned long long time;
		unsigned long sp;
		struct worker *worker;
		char task_comm[TASK_COMM_LEN];
		work_func_t fn;
		int en;
	} work[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct cpuidle_log {
		unsigned long long time;
		unsigned long sp;
		char *modes;
		unsigned state;
		u32 num_online_cpus;
		int delta;
		int en;
	} cpuidle[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct suspend_log {
		unsigned long long time;
		unsigned long sp;
		void *fn;
		struct device *dev;
		int en;
		int core;
	} suspend[ESS_LOG_MAX_NUM * 4];

	struct irq_log {
		unsigned long long time;
		unsigned long sp;
		int irq;
		void *fn;
		unsigned int preempt;
		unsigned int val;
		int en;
	} irq[ESS_NR_CPUS][ESS_LOG_MAX_NUM * 2];

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	struct clockevent_log {
		unsigned long long time;
		unsigned long long mct_cycle;
		int64_t	delta_ns;
		ktime_t	next_event;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} clockevent[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct printkl_log {
		unsigned long long time;
		int cpu;
		size_t msg;
		size_t val;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printkl[ESS_API_MAX_NUM];

	struct printk_log {
		unsigned long long time;
		int cpu;
		char log[ESS_LOG_STRING_LENGTH];
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printk[ESS_API_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_CORESIGHT
	struct core_log {
		void *last_pc[ESS_ITERATION];
	} core[ESS_NR_CPUS];
#endif
};

#define ESS_SAVE_STACK_TRACE_CPU(xxx)					\
	do {								\
		struct stack_trace t = {				\
			.nr_entries = 0,				\
			.max_entries = ess_desc.callstack,		\
			.entries = (unsigned long *)ess_log->xxx[cpu][i].caller, \
			.skip = 3					\
		};							\
		save_stack_trace(&t);					\
	} while (0)

#define ESS_SAVE_STACK_TRACE(xxx)					\
	do {								\
		struct stack_trace t = {				\
			.nr_entries = 0,				\
			.max_entries = ess_desc.callstack,		\
			.entries = (unsigned long *)ess_log->xxx[i].caller, \
			.skip = 3					\
		};							\
		save_stack_trace(&t);					\
	} while (0)

struct exynos_ss_log_idx {
	atomic_t task_log_idx[ESS_NR_CPUS];
	atomic_t work_log_idx[ESS_NR_CPUS];
	atomic_t cpuidle_log_idx[ESS_NR_CPUS];
	atomic_t suspend_log_idx;
	atomic_t irq_log_idx[ESS_NR_CPUS];
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_t clockevent_log_idx[ESS_NR_CPUS];
	atomic_t printkl_log_idx;
	atomic_t printk_log_idx;
#endif
};
#ifdef CONFIG_ARM64
struct exynos_ss_mmu_reg {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
	long ELR_EL1;
};

#else
struct exynos_ss_mmu_reg {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};
#endif

struct exynos_ss_desc {
	raw_spinlock_t lock;
	unsigned int kevents_num;
	unsigned int log_kernel_num;
	unsigned int log_platform_num;
	unsigned int log_sfr_num;
//	unsigned int log_pstore_num;
	unsigned int log_etm_num;
	bool need_header;

	unsigned int callstack;
	unsigned long hardlockup_core_mask;
	unsigned long hardlockup_core_pc[ESS_NR_CPUS];
	int hardlockup;
	int no_wdt_dev;

	struct vm_struct vm;
};

struct exynos_ss_interface {
	struct exynos_ss_log *info_event;
	struct exynos_ss_item info_log[ESS_ITEM_MAX_NUM];
};

#ifdef CONFIG_S3C2410_WATCHDOG
extern int s3c2410wdt_set_emergency_stop(void);
extern int s3c2410wdt_set_emergency_reset(unsigned int timeout);
extern int s3c2410wdt_keepalive_emergency(bool reset);
#else
#define s3c2410wdt_set_emergency_stop() 	(-1)
#define s3c2410wdt_set_emergency_reset(a)	do { } while(0)
#define s3c2410wdt_keepalive_emergency(a)	do { } while(0)
#endif
extern void *return_address(int);
extern void (*arm_pm_restart)(char str, const char *cmd);
#ifdef CONFIG_EXYNOS_CORESIGHT_PC_INFO
extern unsigned long exynos_cs_pc[NR_CPUS][ESS_ITERATION];
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
extern void register_hook_logbuf(void (*)(const char));
#else
extern void register_hook_logbuf(void (*)(const char *, size_t));
#endif
extern void register_hook_logger(void (*)(const char *, const char *, size_t));

typedef int (*ess_initcall_t)(const struct device_node *);

/*
 * ---------------------------------------------------------------------------
 *  User defined Start
 * ---------------------------------------------------------------------------
 *
 *  clarified exynos-snapshot items, before using exynos-snapshot we should
 *  evince memory-map of snapshot
 */
static struct exynos_ss_item ess_items[] = {
/*****************************************************************/
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	{"log_kevents",	{SZ_8M,		0, 0, false, true, true}, NULL ,NULL, 0},
	{"log_kernel",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	{"log_etm",	{SZ_8M,		0, 0, true, true, true}, NULL ,NULL, 0},
#endif
#else /* MINIMIZED MODE */
	{"log_kevents",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
	{"log_kernel",	{SZ_2M,		0, 0, false, true, true}, NULL ,NULL, 0},
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PSTORE
//	{"testing",	{SZ_32K,	0, 0, true, true, true}, NULL ,NULL, 0},
#endif

};

/*
 * ---------------------------------------------------------------------------
 *  User defined End
 * ---------------------------------------------------------------------------
 */

/*  External interface variable for trace debugging */
static struct exynos_ss_interface ess_info;

/*  Internal interface variable */
static struct exynos_ss_base ess_base;
static struct exynos_ss_log_idx ess_idx;
static struct exynos_ss_log *ess_log = NULL;
static struct exynos_ss_desc ess_desc;

DEFINE_PER_CPU(struct pt_regs *, ess_core_reg);
DEFINE_PER_CPU(struct exynos_ss_mmu_reg *, ess_mmu_reg);

void __iomem *exynos_ss_get_base_vaddr(void)
{
	return (void __iomem *)(ess_base.vaddr);
}

static void exynos_ss_scratch_reg(unsigned int val)
{
	if (exynos_ss_get_enable("log_kevents", true) || ess_desc.need_header)
		__raw_writel(val, exynos_ss_get_base_vaddr() + ESS_OFFSET_SCRATCH);
}

unsigned int exynos_ss_get_item_paddr(char* name)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.paddr;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_paddr);

int exynos_ss_set_enable(const char *name, int en)
{
	if (!strncmp(name, "base", strlen(name))) {
		ess_base.enabled = en;
		pr_info("exynos-snapshot: %sabled\n", en ? "en" : "dis");
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_enable);

int exynos_ss_get_enable(const char *name, bool init)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;
	int ret = -1;

	if (!strncmp(name, "base", strlen(name))) {
		ret = ess_base.enabled;
	} else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				if (init)
					ret = item->entry.enabled_init;
				else
					ret = item->entry.enabled;
				break;
			}
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_get_enable);

static inline int exynos_ss_check_eob(struct exynos_ss_item *item,
						size_t size)
{
	size_t max, cur;

	max = (size_t)(item->head_ptr + item->entry.size);
	cur = (size_t)(item->curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
static inline void exynos_ss_hook_logbuf(const char buf)
{
	unsigned int last_buf;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		if (exynos_ss_check_eob(item, 1)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
		}

		item->curr_ptr[0] = buf;
		item->curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)item->curr_ptr;
		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#else
static inline void exynos_ss_hook_logbuf(const char *buf, size_t size)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (likely(ess_base.enabled == true && item->entry.enabled == true)) {
		size_t last_buf;

		if (exynos_ss_check_eob(item, size)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
		}

		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
		/*  save the address of last_buf to physical address */
		last_buf = (size_t)item->curr_ptr;

		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#endif

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

struct vclk {
	unsigned int type;
	struct vclk *parent;
	int ref_count;
	unsigned long vfreq;
	char *name;
};

static size_t __init exynos_ss_remap(void)
{
	unsigned long i;
	unsigned int enabled_count = 0;
	size_t pre_paddr, pre_vaddr, item_size;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size, ret;
	struct page *page;
	struct page **pages;

	page_size = ess_desc.vm.size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page*) * page_size, GFP_KERNEL);
	page = phys_to_page(ess_desc.vm.phys_addr);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	ret = map_vm_area(&ess_desc.vm, prot, pages);
	kfree(pages);
	if (ret) {
		pr_err("exynos-snapshot: failed to mapping between virt and phys for firmware");
		return -ENOMEM;
	}

	/* initializing value */
	pre_paddr = (size_t)ess_base.paddr;
	pre_vaddr = (size_t)ess_base.vaddr;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		/* fill rest value of ess_items arrary */
		if (i == ess_desc.kevents_num ||
			ess_items[i].entry.enabled_init) {

			if (i == ess_desc.kevents_num && ess_desc.need_header)
				item_size = ESS_HEADER_ALLOC_SZ;
			else
				item_size = ess_items[i].entry.size;

			ess_items[i].entry.vaddr = pre_vaddr;
			ess_items[i].entry.paddr = pre_paddr;

			ess_items[i].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
			ess_items[i].curr_ptr = (unsigned char *)ess_items[i].entry.vaddr;

			/* For Next */
			pre_vaddr = ess_items[i].entry.vaddr + item_size;
			pre_paddr = ess_items[i].entry.paddr + item_size;

			enabled_count++;
		}
	}
	return (size_t)(enabled_count ? exynos_ss_get_base_vaddr() : 0);
}

static int __init exynos_ss_init_desc(void)
{
	unsigned int i, len;

	/* initialize ess_desc */
	memset((struct exynos_ss_desc *)&ess_desc, 0, sizeof(struct exynos_ss_desc));
	ess_desc.callstack = CONFIG_EXYNOS_SNAPSHOT_CALLSTACK;
	raw_spin_lock_init(&ess_desc.lock);

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		len = strlen(ess_items[i].name);
		if (!strncmp(ess_items[i].name, "log_kevents", len))
			ess_desc.kevents_num = i;
		else if (!strncmp(ess_items[i].name, "log_kernel", len))
			ess_desc.log_kernel_num = i;
		else if (!strncmp(ess_items[i].name, "log_platform", len))
			ess_desc.log_platform_num = i;
		else if (!strncmp(ess_items[i].name, "log_sfr", len))
			ess_desc.log_sfr_num = i;
		else if (!strncmp(ess_items[i].name, "log_etm", len))
			ess_desc.log_etm_num = i;
	}

	if (!ess_items[ess_desc.kevents_num].entry.enabled_init)
		ess_desc.need_header = true;

#ifdef CONFIG_S3C2410_WATCHDOG
	ess_desc.no_wdt_dev = false;
#else
	ess_desc.no_wdt_dev = true;
#endif
	return 0;
}

static int __init exynos_ss_setup(char *str)
{
	unsigned long i;
	size_t size = 0;
	size_t base = 0;

	if (kstrtoul(str, 0, (unsigned long *)&base))
		goto out;

	exynos_ss_init_desc();

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (ess_items[i].entry.enabled_init)
			size += ess_items[i].entry.size;
	}
	size += SZ_32K;

	/* More need the size for Header */
	if (ess_desc.need_header)
		size += ESS_HEADER_ALLOC_SZ;

	pr_info("exynos-snapshot: try to reserve dedicated memory : 0x%zx, 0x%zx\n",
			base, size);

#ifdef CONFIG_NO_BOOTMEM
	if (!memblock_is_region_reserved(base, size) &&
		!memblock_reserve(base, size)) {
#else
	if (!reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		ess_base.paddr = base;
		ess_base.vaddr = (size_t)(ESS_FIXED_VIRT_BASE);
		ess_base.size = size;
		ess_base.enabled = false;

		/* Reserved fixed virtual memory within VMALLOC region */
		ess_desc.vm.phys_addr = base;
		ess_desc.vm.addr = (void *)(ESS_FIXED_VIRT_BASE);
		ess_desc.vm.size = size;

		vm_area_add_early(&ess_desc.vm);

		pr_info("exynos-snapshot: memory reserved complete : 0x%zx, 0x%zx, 0x%zx\n",
			base, (size_t)(ESS_FIXED_VIRT_BASE), size);

		return 0;
	}
out:
	pr_err("exynos-snapshot: buffer reserved failed : 0x%zx, 0x%zx\n", base, size);
	return -1;
}
__setup("ess_setup=", exynos_ss_setup);

/*
 *  Normally, exynos-snapshot has 2-types debug buffer - log and hook.
 *  hooked buffer is for log_buf of kernel and loggers of platform.
 *  Each buffer has 2Mbyte memory except loggers. Loggers is consist of 4
 *  division. Each logger has 1Mbytes.
 *  ---------------------------------------------------------------------
 *  - dummy data:phy_addr, virtual_addr, buffer_size, magic_key(4K)	-
 *  ---------------------------------------------------------------------
 *  -		Cores MMU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		Cores CPU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		log buffer(3Mbyte - Headers(12K))			-
 *  ---------------------------------------------------------------------
 *  -		Hooked buffer of kernel's log_buf(2Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked main logger buffer of platform(3Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked system logger buffer of platform(1Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked radio logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 *  -		Hooked events logger buffer of platform(?Mbyte)		-
 *  ---------------------------------------------------------------------
 */
static int __init exynos_ss_output(void)
{
	unsigned long i;

	pr_info("exynos-snapshot physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(ess_items); i++)
		if (ess_items[i].entry.enabled_init)
			pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
				ess_items[i].name,
				ess_items[i].entry.paddr,
				ess_items[i].entry.vaddr,
				ess_items[i].entry.size);

	return 0;
}

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	vaddr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/
static void __init exynos_ss_fixmap_header(void)
{
	/*  fill 0 to next to header */
	size_t vaddr, paddr, size;
	size_t *addr;
	int i;

	vaddr = ess_items[ess_desc.kevents_num].entry.vaddr;
	paddr = ess_items[ess_desc.kevents_num].entry.paddr;
	size = ess_items[ess_desc.kevents_num].entry.size;

	/*  set to confirm exynos-snapshot */
	addr = (size_t *)vaddr;
	memcpy(addr, &ess_base, sizeof(struct exynos_ss_base));

	for (i = 0; i < ESS_NR_CPUS; i++) {
		per_cpu(ess_mmu_reg, i) = (struct exynos_ss_mmu_reg *)
					  (vaddr + ESS_HEADER_SZ +
					   i * ESS_MMU_REG_OFFSET);
		per_cpu(ess_core_reg, i) = (struct pt_regs *)
					   (vaddr + ESS_HEADER_SZ + ESS_MMU_REG_SZ +
					    i * ESS_CORE_REG_OFFSET);
	}

	if (!exynos_ss_get_enable("log_kevents", true))
		return;

	/*  kernel log buf */
	ess_log = (struct exynos_ss_log *)(vaddr + ESS_HEADER_TOTAL_SZ);

	/*  set fake translation to virtual address to debug trace */
	ess_info.info_event = (struct exynos_ss_log *)ess_log;

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_set(&(ess_idx.printk_log_idx), -1);
	atomic_set(&(ess_idx.printkl_log_idx), -1);
#endif
	atomic_set(&(ess_idx.suspend_log_idx), -1);

	for (i = 0; i < ESS_NR_CPUS; i++) {
		atomic_set(&(ess_idx.task_log_idx[i]), -1);
		atomic_set(&(ess_idx.work_log_idx[i]), -1);
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
		atomic_set(&(ess_idx.clockevent_log_idx[i]), -1);
#endif
		atomic_set(&(ess_idx.cpuidle_log_idx[i]), -1);
		atomic_set(&(ess_idx.irq_log_idx[i]), -1);
	}
	/*  initialize kernel event to 0 except only header */
	memset((size_t *)(vaddr + ESS_KEEP_HEADER_SZ), 0, size - ESS_KEEP_HEADER_SZ);
}

static int __init exynos_ss_fixmap(void)
{
	size_t last_buf;
	size_t vaddr, paddr, size;
	unsigned long i;

	/*  fixmap to header first */
	exynos_ss_fixmap_header();

	for (i = 1; i < ARRAY_SIZE(ess_items); i++) {
		if (!ess_items[i].entry.enabled_init)
			continue;

		/*  assign kernel log information */
		paddr = ess_items[i].entry.paddr;
		vaddr = ess_items[i].entry.vaddr;
		size = ess_items[i].entry.size;

		if (!strncmp(ess_items[i].name, "log_kernel", strlen(ess_items[i].name))) {
			/*  load last_buf address value(phy) by virt address */
			last_buf = (size_t)__raw_readl(exynos_ss_get_base_vaddr() +
							ESS_OFFSET_LAST_LOGBUF);
			/*  check physical address offset of kernel logbuf */
			if (last_buf >= ess_items[i].entry.paddr &&
				(last_buf) <= (ess_items[i].entry.paddr + ess_items[i].entry.size)) {
				/*  assumed valid address, conversion to virt */
				ess_items[i].curr_ptr = (unsigned char *)(ess_items[i].entry.vaddr +
							(last_buf - ess_items[i].entry.paddr));
			} else {
				/*  invalid address, set to first line */
				ess_items[i].curr_ptr = (unsigned char *)vaddr;
				/*  initialize logbuf to 0 */
				memset((size_t *)vaddr, 0, size);
			}
		} else {
			/*  initialized log to 0 if persist == false */
			if (ess_items[i].entry.persist == false)
				memset((size_t *)vaddr, 0, size);
		}
		ess_info.info_log[i - 1].name = kstrdup(ess_items[i].name, GFP_KERNEL);
		ess_info.info_log[i - 1].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
		ess_info.info_log[i - 1].curr_ptr = NULL;
		ess_info.info_log[i - 1].entry.size = size;
	}

	/* output the information of exynos-snapshot */
	exynos_ss_output();
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
	sec_debug_save_last_kmsg(ess_items[ess_desc.log_kernel_num].head_ptr, 
				ess_items[ess_desc.log_kernel_num].curr_ptr, ess_items[ess_desc.log_kernel_num].entry.size);
#endif
	return 0;
}

static int __init exynos_ss_init(void)
{
	if (ess_base.vaddr && ess_base.paddr && ess_base.size) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		exynos_ss_remap();
		exynos_ss_fixmap();
		exynos_ss_scratch_reg(ESS_SIGN_SCRATCH);
		exynos_ss_set_enable("base", true);

		register_hook_logbuf(exynos_ss_hook_logbuf);
	} else
		pr_err("exynos-snapshot: %s failed\n", __func__);

	return 0;
}
early_initcall(exynos_ss_init);
