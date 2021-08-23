/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_DECON_H__
#define ___SAMSUNG_DECON_H__

#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-core.h>
#include <soc/samsung/exynos-itmon.h>

#include "regs-decon.h"
#include "dsim.h"

#define MAX_DECON_CNT		3
#define SUCCESS_EXYNOS_SMC	0

extern struct ion_device *ion_exynos;
extern struct decon_device *decon_drvdata[MAX_DECON_CNT];
extern int decon_log_level;
extern int win_update_log_level;

#define DECON_MODULE_NAME	"exynos-decon"
#define MAX_NAME_SIZE		32
#define MAX_PLANE_CNT		3
#define DEFAULT_BPP		32

#define MAX_DECON_WIN		4

#define decon_err(fmt, ...)							\
	do {									\
		if (decon_log_level >= 3) {					\
			pr_err(pr_fmt("decon: "fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_warn(fmt, ...)							\
	do {									\
		if (decon_log_level >= 4) {					\
			pr_warn(pr_fmt("decon: "fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define decon_info(fmt, ...)							\
	do {									\
		if (decon_log_level >= 6)					\
			pr_info(pr_fmt("decon: "fmt), ##__VA_ARGS__);			\
	} while (0)

#define decon_dbg(fmt, ...)							\
	do {									\
		if (decon_log_level >= 7)					\
			pr_info(pr_fmt("decon: "fmt), ##__VA_ARGS__);			\
	} while (0)

enum decon_trig_mode {
	DECON_HW_TRIG = 0,
	DECON_SW_TRIG
};

enum decon_out_type {
	DECON_OUT_DSI = 0,
	DECON_OUT_EDP,
	DECON_OUT_DP,
	DECON_OUT_WB
};

enum decon_dsi_mode {
	DSI_MODE_SINGLE = 0,
	DSI_MODE_DUAL_DSI,
	DSI_MODE_DUAL_DISPLAY,
	DSI_MODE_NONE
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

enum decon_idma_type {
	IDMA_G0 = 0,	/* Dedicated to WIN3 */
	IDMA_G1,
	IDMA_VG0,
	IDMA_GF,
	/* this will be removed later */
	/* start */
	IDMA_VG1,
	IDMA_VGF0,
	IDMA_VGF1,
	ODMA_WB,
	/* end */
	IDMA_G0_S,
#if defined(CONFIG_DPU_20)
	IDMA_VGS0,
#endif
};

/*
 * DECON_STATE_ON : disp power on, decon/dsim clock on & lcd on
 * DECON_HIBER : disp power off, decon/dsim clock off & lcd on
 * DECON_STATE_OFF : disp power off, decon/dsim clock off & lcd off
 */
enum decon_state {
	DECON_STATE_INIT = 0,
	DECON_STATE_ON,
	DECON_STATE_HIBER,
	DECON_STATE_OFF,
	DECON_STATE_TUI,
};

#if defined(CONFIG_DPU_20)
enum decon_pixel_format {
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,
	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_BGRA_5551,
	DECON_PIXEL_FORMAT_ABGR_4444,
	DECON_PIXEL_FORMAT_RGBA_4444,
	DECON_PIXEL_FORMAT_BGRA_4444,
	DECON_PIXEL_FORMAT_RGB_565,
	DECON_PIXEL_FORMAT_BGR_565,
	DECON_PIXEL_FORMAT_ARGB_2101010,
	DECON_PIXEL_FORMAT_ABGR_2101010,
	DECON_PIXEL_FORMAT_RGBA_1010102,
	DECON_PIXEL_FORMAT_BGRA_1010102,
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,
	DECON_PIXEL_FORMAT_YVU422_3P,
	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,
	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,
	DECON_PIXEL_FORMAT_NV12N,
	DECON_PIXEL_FORMAT_NV12N_10B,
	DECON_PIXEL_FORMAT_NV12M_P010,
	DECON_PIXEL_FORMAT_NV21M_P010,
	DECON_PIXEL_FORMAT_NV12M_S10B,
	DECON_PIXEL_FORMAT_NV21M_S10B,
	DECON_PIXEL_FORMAT_NV16M_P210,
	DECON_PIXEL_FORMAT_NV61M_P210,
	DECON_PIXEL_FORMAT_NV16M_S10B,
	DECON_PIXEL_FORMAT_NV61M_S10B,
	DECON_PIXEL_FORMAT_NV12_P010,
	DECON_PIXEL_FORMAT_MAX,
};
#else
enum decon_pixel_format {
	DECON_PIXEL_FORMAT_ARGB_8888 = 0,
	DECON_PIXEL_FORMAT_ABGR_8888,
	DECON_PIXEL_FORMAT_RGBA_8888,
	DECON_PIXEL_FORMAT_BGRA_8888,
	DECON_PIXEL_FORMAT_XRGB_8888,
	DECON_PIXEL_FORMAT_XBGR_8888,
	DECON_PIXEL_FORMAT_RGBX_8888,
	DECON_PIXEL_FORMAT_BGRX_8888,

	DECON_PIXEL_FORMAT_RGBA_5551,
	DECON_PIXEL_FORMAT_RGB_565,
	DECON_PIXEL_FORMAT_NV16,
	DECON_PIXEL_FORMAT_NV61,

	DECON_PIXEL_FORMAT_YVU422_3P,

	DECON_PIXEL_FORMAT_NV12,
	DECON_PIXEL_FORMAT_NV21,
	DECON_PIXEL_FORMAT_NV12M,
	DECON_PIXEL_FORMAT_NV21M,

	DECON_PIXEL_FORMAT_YUV420,
	DECON_PIXEL_FORMAT_YVU420,
	DECON_PIXEL_FORMAT_YUV420M,
	DECON_PIXEL_FORMAT_YVU420M,
	DECON_PIXEL_FORMAT_NV12N,
	DECON_PIXEL_FORMAT_MAX,
};
#endif

#if defined(CONFIG_DPU_20)
enum dpp_flip {
	DPP_FLIP_NONE = 0x0,
	DPP_FLIP_X,
	DPP_FLIP_Y,
	DPP_FLIP_XY,
	DPP_ROT_90,
	DPP_ROT_90_XFLIP,
	DPP_ROT_90_YFLIP,
	DPP_ROT_270,
};
#else
enum dpp_flip {
	DPP_FLIP_NONE = 0x0,
	DPP_FLIP_X,
	DPP_FLIP_Y,
	DPP_FLIP_XY,
};
#endif

#if defined(CONFIG_DPU_20)
enum dpp_csc_eq {
	/* eq_mode : 6bits [5:0] */
	CSC_STANDARD_SHIFT = 0,
	CSC_BT_601 = 0,
	CSC_BT_709 = 1,
	CSC_BT_2020 = 2,
	CSC_DCI_P3 = 3,
	CSC_BT_601_625,
	CSC_BT_601_625_UNADJUSTED,
	CSC_BT_601_525,
	CSC_BT_601_525_UNADJUSTED,
	CSC_BT_2020_CONSTANT_LUMINANCE,
	CSC_BT_470M,
	CSC_FILM,
	CSC_ADOBE_RGB,
	CSC_UNSPECIFIED = 63,
	/* eq_mode : 3bits [8:6] */
	CSC_RANGE_SHIFT = 6,
	CSC_RANGE_LIMITED = 0x0,
	CSC_RANGE_FULL = 0x1,
	CSC_RANGE_EXTENDED,
	CSC_RANGE_UNSPECIFIED = 7
};
#else
enum dpp_csc_eq {
	/* eq_mode : 6bits [5:0] */
	CSC_STANDARD_SHIFT = 0,
	CSC_BT_601 = 0,
	CSC_BT_709 = 1,
	CSC_BT_2020 = 2,
	CSC_DCI_P3 = 3,
	/* eq_mode : 3bits [8:6] */
	CSC_RANGE_SHIFT = 6,
	CSC_RANGE_LIMITED = 0x0,
	CSC_RANGE_FULL = 0x1,
};
#endif

struct decon_mode_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_out_type out_type;
	enum decon_dsi_mode dsi_mode;
};

struct decon_param {
	struct decon_mode_info psr;
	struct decon_lcd *lcd_info;
	u32 nr_windows;
	void __iomem *disp_ss_regs;
};

struct decon_dma_buf_data {
	struct ion_handle		*ion_handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct sync_fence		*fence;
};

struct decon_win_rect {
};

struct decon_rect {
	u32 left;
	u32 top;
	u32 right;
	u32 bottom;
};

struct dpp_params {
	dma_addr_t addr[MAX_PLANE_CNT];
};

struct decon_frame {
	u32 w;
	u32 h;
};

struct decon_win_config {
	/* Reusability:This struct is used for IDMA and ODMA */
	union {
		struct {
			enum decon_pixel_format		format;
			struct dpp_params		dpp_parm;
			struct decon_frame		src;
		};
	};
	struct decon_frame dst;
};

struct decon_reg_data {
	struct sync_pt *pt;
};

struct decon_resources {
	void __iomem *regs;
	void __iomem *ss_regs;
};

struct decon_dt_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_dsi_mode dsi_mode;
	enum decon_out_type out_type;
	int out_idx[MAX_DSIM_CNT];
	int max_win;
	int dft_win;
	int dft_idma;
	u32 disp_freq;
	u32 mif_freq;
};

struct decon_win {
	struct decon_device *decon;
	struct fb_info *fbinfo;

	struct fb_videomode videomode;
	struct decon_dma_buf_data dma_buf_data[MAX_PLANE_CNT];
	int plane_cnt;

	int idx;
	u32 pseudo_palette[16];
};

struct decon_debug {
	void __iomem *eint_pend;
};

struct decon_update_regs {
	struct mutex lock;
	struct task_struct *thread;
};

struct decon_vsync {
	wait_queue_head_t wait;
	struct mutex lock;
};

struct decon_hiber {
	void __iomem *cam_status;
};

struct decon_device {
	int id;
	enum decon_state state;

	unsigned long cur_using_dpp;
	unsigned long dpp_err_stat;

	struct mutex lock;
	struct mutex pm_lock;
	spinlock_t slock;

	struct ion_client *ion_client;

	struct v4l2_subdev *dsim_sd[MAX_DSIM_CNT];

	struct device *dev;

	struct decon_dt_info dt;
	struct decon_win *win[MAX_DECON_WIN];
	struct decon_resources res;
	struct decon_debug d;
	struct decon_update_regs up;
	struct decon_vsync vsync;
	struct decon_lcd *lcd_info;
	struct decon_hiber hiber;

	wait_queue_head_t wait_vstatus;

	atomic_t is_shutdown;
};

static inline struct decon_device *get_decon_drvdata(u32 id)
{
	return decon_drvdata[id];
}

/* register access subroutines */
static inline u32 decon_read(u32 id, u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(id);
	return readl(decon->res.regs + reg_id);
}

static inline void decon_write(u32 id, u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(id);
	writel(val, decon->res.regs + reg_id);
}

static inline void decon_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	u32 old = decon_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	decon_write(id, reg_id, val);
}

int decon_setcolreg(unsigned regno,
			    unsigned red, unsigned green, unsigned blue,
			    unsigned transp, struct fb_info *info);
int decon_mmap(struct fb_info *info, struct vm_area_struct *vma);
int decon_sync(struct fb_info *info);

static inline void decon_enter_shutdown_reset(struct decon_device *decon)
{
	atomic_set(&decon->is_shutdown, 0);
}

void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
		enum decon_set_trig en);
void decon_reg_update_req_window(u32 id, u32 win_idx);
void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr);

#endif /* ___SAMSUNG_DECON_H__ */
