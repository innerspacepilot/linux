/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/fb.h>

#include "decon.h"
#include "dsim.h"
#include "dpp.h"

int decon_log_level = 0;
module_param(decon_log_level, int, 0644);

struct decon_device *decon_drvdata[MAX_DECON_CNT] = {NULL, NULL, NULL};
EXPORT_SYMBOL(decon_drvdata);

extern int dpp_set_config_ext(struct dpp_device *dpp, struct decon_win_config *arg);

/* ---------- FREAMBUFFER INTERFACE ----------- */
static struct fb_ops decon_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= decon_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_sync	= decon_sync,
};

static void decon_release_windows(struct decon_win *win)
{
	if (win->fbinfo)
		framebuffer_release(win->fbinfo);
}

dma_addr_t dpp_addr;

static int decon_fb_alloc_memory(struct decon_device *decon, struct decon_win *win)
{
	struct decon_lcd *lcd_info = &dsim_drvdata[0]->lcd_info;
	struct fb_info *fbi = win->fbinfo;
	unsigned int size;
	dma_addr_t map_dma;

	decon_info("%s +\n", __func__);

	dev_info(decon->dev, "allocating memory for display\n");

	size = lcd_info->xres * lcd_info->yres * DEFAULT_BPP / 8;

	fbi->fix.smem_len = size;
	size = PAGE_ALIGN(size);

	dev_info(decon->dev, "want %u bytes for window[%d]\n", size, win->idx);

	fbi->screen_base = dma_alloc_wc(decon->dev, size,
						  &map_dma, GFP_KERNEL);

	dev_info(decon->dev, "mapped %x to %x\n",
		(unsigned int)map_dma, fbi->screen_base);

	memset(fbi->screen_base, 0x0, size);

	fbi->fix.smem_start = map_dma;
	dpp_addr = map_dma;

	dev_info(decon->dev, "fb start addr = 0x%x\n", (u32)fbi->fix.smem_start);
	decon_info("%s -\n", __func__);

	return 0;
}

static int decon_acquire_window(struct decon_device *decon, int idx)
{
	struct decon_win *win;
	struct fb_info *fbinfo;
	struct fb_var_screeninfo *var;
	struct decon_lcd *lcd_info = &dsim_drvdata[0]->lcd_info;
	int ret;

	decon_dbg("acquire DECON window%d\n", idx);

	fbinfo = framebuffer_alloc(sizeof(struct decon_win), decon->dev);
	if (!fbinfo) {
		decon_err("failed to allocate framebuffer\n");
		return -ENOENT;
	}

	win = fbinfo->par;
	decon->win[idx] = win;
	var = &fbinfo->var;
	win->fbinfo = fbinfo;
	win->decon = decon;
	win->idx = idx;

	if (decon->dt.out_type == DECON_OUT_DSI
		|| decon->dt.out_type == DECON_OUT_DP) {
		win->videomode.left_margin = lcd_info->hbp;
		win->videomode.right_margin = lcd_info->hfp;
		win->videomode.upper_margin = lcd_info->vbp;
		win->videomode.lower_margin = lcd_info->vfp;
		win->videomode.hsync_len = lcd_info->hsa;
		win->videomode.vsync_len = lcd_info->vsa;
		win->videomode.xres = lcd_info->xres;
		win->videomode.yres = lcd_info->yres;
		fb_videomode_to_var(&fbinfo->var, &win->videomode);
	}

	if (((decon->dt.out_type == DECON_OUT_DSI) || (decon->dt.out_type == DECON_OUT_DP))
			&& (idx == decon->dt.dft_win)) {
		ret = decon_fb_alloc_memory(decon, win);
		if (ret) {
			dev_err(decon->dev, "failed to allocate display memory\n");
			return ret;
		}
	}

	fbinfo->fix.type	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.visual	= FB_VISUAL_TRUECOLOR,
	fbinfo->fix.accel	= FB_ACCEL_NONE;
	fbinfo->var.activate	= FB_ACTIVATE_NOW;
	fbinfo->var.vmode	= FB_VMODE_NONINTERLACED;
	fbinfo->var.bits_per_pixel = DEFAULT_BPP;
	fbinfo->var.width	= lcd_info->width;
	fbinfo->var.height	= lcd_info->height;
	fbinfo->fbops		= &decon_fb_ops;
	fbinfo->flags		= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  = &win->pseudo_palette;

	/* 'divide by 8' means converting bit to byte number */
	fbinfo->fix.line_length = fbinfo->var.xres * fbinfo->var.bits_per_pixel / 8;
	decon_info("default_win %d win_idx %d xres %d yres %d\n",
			decon->dt.dft_win, idx,
			fbinfo->var.xres, fbinfo->var.yres);

	fbinfo->var.transp.length	= var->bits_per_pixel - 24;
	fbinfo->var.transp.offset	= 24;
	fbinfo->var.bits_per_pixel	= 32;
	fbinfo->var.red.offset		= 16;
	fbinfo->var.red.length		= 8;
	fbinfo->var.green.offset	= 8;
	fbinfo->var.green.length	= 8;
	fbinfo->var.blue.offset		= 0;
	fbinfo->var.blue.length		= 8;

	decon_dbg("decon%d window[%d] create\n", decon->id, idx);
	return 0;
}

static int decon_acquire_windows(struct decon_device *decon)
{
	int i, ret;

	i = decon->dt.dft_win;
	ret = decon_acquire_window(decon, i);
	if (ret < 0) {
		decon_err("failed to create decon-int window[%d]\n", i);
		decon_release_windows(decon->win[i]);
		return ret;
	}

	ret = register_framebuffer(decon->win[decon->dt.dft_win]->fbinfo);
	if (ret) {
		decon_err("failed to register framebuffer\n");
		return ret;
	}

	return 0;
}

static void decon_parse_dt(struct decon_device *decon)
{
	struct device_node *te_eint;
	struct device_node *cam_stat;
	struct device *dev = decon->dev;
	int ret = 0;

	if (!dev->of_node) {
		decon_warn("no device tree information\n");
		return;
	}

	decon->id = of_alias_get_id(dev->of_node, "decon");
	of_property_read_u32(dev->of_node, "max_win",
			&decon->dt.max_win);
	of_property_read_u32(dev->of_node, "default_win",
			&decon->dt.dft_win);
	of_property_read_u32(dev->of_node, "default_idma",
			&decon->dt.dft_idma);
	/* video mode: 0, dp: 1 mipi command mode: 2 */
	of_property_read_u32(dev->of_node, "psr_mode",
			&decon->dt.psr_mode);
	/* H/W trigger: 0, S/W trigger: 1 */
	of_property_read_u32(dev->of_node, "trig_mode",
			&decon->dt.trig_mode);
	decon_info("decon-%s: max win%d, %s mode, %s trigger\n",
			(decon->id == 0) ? "f" : ((decon->id == 1) ? "s" : "t"),
			decon->dt.max_win,
			decon->dt.psr_mode ? "command" : "video",
			decon->dt.trig_mode ? "sw" : "hw");

	/* 0: DSI_MODE_SINGLE, 1: DSI_MODE_DUAL_DSI */
	of_property_read_u32(dev->of_node, "dsi_mode", &decon->dt.dsi_mode);
	decon_info("dsi mode(%d). 0: SINGLE 1: DUAL\n", decon->dt.dsi_mode);

	of_property_read_u32(dev->of_node, "out_type", &decon->dt.out_type);
	decon_info("out type(%d). 0: DSI 1: DISPLAYPORT 2: HDMI 3: WB\n",
			decon->dt.out_type);

	if (decon->dt.out_type == DECON_OUT_DSI) {
		ret = of_property_read_u32_index(dev->of_node, "out_idx", 0,
				&decon->dt.out_idx[0]);
		if (!ret)
			decon_info("out idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
					decon->dt.out_idx[0]);
		else
			decon_err("decon out idx is Empty\n");

		if (decon->dt.dsi_mode == DSI_MODE_DUAL_DSI) {
			ret = of_property_read_u32_index(dev->of_node, "out_idx", 1,
					&decon->dt.out_idx[1]);
			if (!ret)
				decon_info("out1 idx(%d). 0: DSI0 1: DSI1 2: DSI2\n",
						decon->dt.out_idx[1]);
			else
				decon_err("decon out1 idx is Empty\n");
		}
	}


	if(!of_property_read_u32(dev->of_node, "disp_freq", &decon->dt.disp_freq))
		decon_info("disp_freq(Khz): %u\n", decon->dt.disp_freq);
	else {
		decon->dt.disp_freq = 0;
		decon_info("disp_freq is not found in dt\n");
	}
	decon->dt.mif_freq = 0;

	if (decon->dt.out_type == DECON_OUT_DSI) {
		te_eint = of_get_child_by_name(decon->dev->of_node, "te_eint");
		if (!te_eint) {
			decon_info("No DT node for te_eint\n");
		} else {
			decon->d.eint_pend = of_iomap(te_eint, 0);
			if (!decon->d.eint_pend)
				decon_info("Failed to get te eint pend\n");
		}

		cam_stat = of_get_child_by_name(decon->dev->of_node, "cam-stat");
		if (!cam_stat) {
			decon_info("No DT node for cam_stat\n");
		} else {
			decon->hiber.cam_status = of_iomap(cam_stat, 0);
			if (!decon->hiber.cam_status)
				decon_info("Failed to get CAM0-STAT Reg\n");
		}
	}
}

struct ion_device *iondev;
static int decon_init_resources(struct decon_device *decon,
		struct platform_device *pdev, char *name)
{
	struct resource *res;
	int ret;

	/* Get memory resource and map SFR region. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	decon->res.regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(decon->res.regs)) {
		decon_err("failed to remap register region\n");
		ret = -ENOENT;
		goto err;
	}

	return 0;

err:
	return ret;
}

static int decon_initial_display(struct decon_device *decon, bool is_colormap)
{
	struct decon_win_config config;

#if defined(CONFIG_PM)
	pm_runtime_get_sync(decon->dev);
#else
	decon_runtime_resume(decon->dev);
#endif

	memset(&config, 0, sizeof(struct decon_win_config));
	config.dpp_parm.addr[0] = dpp_addr;

#if !defined(BRINGUP_DECON_BIST)
	if (dpp_set_config_ext(dpp_drvdata[decon->dt.dft_idma], &config)) {
		decon_err("Failed to config DPP-%d\n",
				decon->dt.dft_idma);
		clear_bit(decon->dt.dft_idma, &decon->cur_using_dpp);
		set_bit(decon->dt.dft_idma, &decon->dpp_err_stat);
	}
#endif

	decon_reg_update_req_window(decon->id, decon->dt.dft_win);

	decon->state = DECON_STATE_INIT;

	return 0;
}

/* --------- DRIVER INITIALIZATION ---------- */
static int decon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct decon_device *decon;
	int ret = 0;
	char device_name[MAX_NAME_SIZE];

	dev_info(dev, "%s start\n", __func__);

	decon = devm_kzalloc(dev, sizeof(struct decon_device), GFP_KERNEL);
	if (!decon) {
		decon_err("no memory for decon device\n");
		ret = -ENOMEM;
		goto err;
	}

	dma_set_mask(dev, DMA_BIT_MASK(36));

	decon->dev = dev;
	decon_parse_dt(decon);

	decon_drvdata[decon->id] = decon;

	spin_lock_init(&decon->slock);
	init_waitqueue_head(&decon->vsync.wait);
	init_waitqueue_head(&decon->wait_vstatus);
	mutex_init(&decon->vsync.lock);
	mutex_init(&decon->lock);
	mutex_init(&decon->pm_lock);
	mutex_init(&decon->up.lock);

	decon_enter_shutdown_reset(decon);

	snprintf(device_name, MAX_NAME_SIZE, "decon%d", decon->id);

	ret = decon_init_resources(decon, pdev, device_name);
	if (ret)
		goto err_res;

	ret = decon_acquire_windows(decon);
	if (ret)
		goto err_win;


	platform_set_drvdata(pdev, decon);
	pm_runtime_enable(dev);

#if defined(BRINGUP_DECON_BIST)
	ret = decon_initial_display(decon, true);
#else
	ret = decon_initial_display(decon, false);
#endif
	if (ret) {
		dev_err(decon->dev, "failed to init decon_initial_display\n");
		goto err_display;
	}

	decon_info("decon%d registered successfully\n", decon->id);

	decon->state = DECON_STATE_ON;
	return 0;

err_display:
err_win:
	iounmap(decon->res.ss_regs);
err_res:
	kfree(decon);
err:
	decon_err("decon probe fail");
	return ret;
}

static int decon_remove(struct platform_device *pdev)
{
	struct decon_device *decon = platform_get_drvdata(pdev);
	int i;

	pm_runtime_disable(&pdev->dev);
	unregister_framebuffer(decon->win[0]->fbinfo);

	if (decon->up.thread)
		kthread_stop(decon->up.thread);

	for (i = 0; i < decon->dt.max_win; i++)
		decon_release_windows(decon->win[i]);

	decon_info("remove sucessful\n");
	return 0;
}

static const struct of_device_id decon_of_match[] = {
	{ .compatible = "samsung,exynos7885-decon" },
	{},
};
MODULE_DEVICE_TABLE(of, decon_of_match);

static struct platform_driver decon_driver __refdata = {
	.probe		= decon_probe,
	.remove		= decon_remove,
	.driver = {
		.name	= DECON_MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(decon_of_match),
		.suppress_bind_attrs = true,
	}
};

static int exynos_decon_register(void)
{
	platform_driver_register(&decon_driver);

	return 0;
}

static void exynos_decon_unregister(void)
{
	platform_driver_unregister(&decon_driver);
}
module_init(exynos_decon_register);
module_exit(exynos_decon_unregister);


MODULE_AUTHOR("Jaehoe Yang <jaehoe.yang@samsung.com>");
MODULE_AUTHOR("Yeongran Shin <yr613.shin@samsung.com>");
MODULE_AUTHOR("Minho Kim <m8891.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DECON driver");
MODULE_LICENSE("GPL");
