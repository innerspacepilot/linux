/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <dt-bindings/clock/exynos7885.h>

#include "decon.h"
#include "dsim.h"
#include "regs-dsim.h"

/* #define BRINGUP_DSIM_BIST */
/* #define PHY_FRAMEWORK_DIS */

#if defined(PHY_FRAMEWORK_DIS)
	void __iomem *dphy_isolation;
#endif

int dsim_log_level = 6;

struct dsim_device *dsim_drvdata[MAX_DSIM_CNT];
EXPORT_SYMBOL(dsim_drvdata);

static void dsim_parse_lcd_info(struct dsim_device *dsim)
{
	u32 res[3];
	struct device_node *node;

	node = of_parse_phandle(dsim->dev->of_node, "lcd_info", 0);

	of_property_read_u32(node, "mode", &dsim->lcd_info.mode);
	dsim_dbg("%s mode\n", dsim->lcd_info.mode ? "command" : "video");

	of_property_read_u32_array(node, "resolution", res, 2);
	dsim->lcd_info.xres = res[0];
	dsim->lcd_info.yres = res[1];
	dsim_info("LCD(%s) resolution: xres(%d), yres(%d)\n",
			of_node_full_name(node), res[0], res[1]);

	of_property_read_u32_array(node, "size", res, 2);
	dsim->lcd_info.width = res[0];
	dsim->lcd_info.height = res[1];
	dsim_dbg("LCD size: width(%d), height(%d)\n", res[0], res[1]);

	of_property_read_u32(node, "timing,refresh", &dsim->lcd_info.fps);
	dsim_dbg("LCD refresh rate(%d)\n", dsim->lcd_info.fps);

	of_property_read_u32_array(node, "timing,h-porch", res, 3);
	dsim->lcd_info.hbp = res[0];
	dsim->lcd_info.hfp = res[1];
	dsim->lcd_info.hsa = res[2];
	dsim_dbg("hbp(%d), hfp(%d), hsa(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32_array(node, "timing,v-porch", res, 3);
	dsim->lcd_info.vbp = res[0];
	dsim->lcd_info.vfp = res[1];
	dsim->lcd_info.vsa = res[2];
	dsim_dbg("vbp(%d), vfp(%d), vsa(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32(node, "timing,dsi-hs-clk", &dsim->lcd_info.hs_clk);
	dsim->clks.hs_clk = dsim->lcd_info.hs_clk;
	dsim_dbg("requested hs clock(%d)\n", dsim->lcd_info.hs_clk);

	of_property_read_u32_array(node, "timing,pms", res, 3);
	dsim->lcd_info.dphy_pms.p = res[0];
	dsim->lcd_info.dphy_pms.m = res[1];
	dsim->lcd_info.dphy_pms.s = res[2];
	dsim_dbg("p(%d), m(%d), s(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32(node, "timing,dsi-escape-clk",
			&dsim->lcd_info.esc_clk);
	dsim->clks.esc_clk = dsim->lcd_info.esc_clk;
	dsim_dbg("requested escape clock(%d)\n", dsim->lcd_info.esc_clk);

	of_property_read_u32(node, "mic_en", &dsim->lcd_info.mic_enabled);
	dsim_info("mic enabled (%d)\n", dsim->lcd_info.mic_enabled);

	of_property_read_u32(node, "type_of_ddi", &dsim->lcd_info.ddi_type);
	dsim_dbg("ddi type(%d)\n", dsim->lcd_info.ddi_type);

	of_property_read_u32(node, "dsc_en", &dsim->lcd_info.dsc_enabled);
	dsim_info("dsc is %s\n", dsim->lcd_info.dsc_enabled ? "enabled" : "disabled");

	of_property_read_u32(node, "eotp_disabled", &dsim->lcd_info.eotp_disabled);
	dsim_info("eotp is %s\n", dsim->lcd_info.eotp_disabled ? "disabled" : "enabled");

	if (dsim->lcd_info.dsc_enabled) {
		of_property_read_u32(node, "dsc_cnt", &dsim->lcd_info.dsc_cnt);
		dsim_info("dsc count(%d)\n", dsim->lcd_info.dsc_cnt);
		of_property_read_u32(node, "dsc_slice_num",
				&dsim->lcd_info.dsc_slice_num);
		dsim_info("dsc slice count(%d)\n", dsim->lcd_info.dsc_slice_num);
		of_property_read_u32(node, "dsc_slice_h",
				&dsim->lcd_info.dsc_slice_h);
		dsim_info("dsc slice height(%d)\n", dsim->lcd_info.dsc_slice_h);
	}

	of_property_read_u32(node, "data_lane", &dsim->data_lane_cnt);
	dsim_info("using data lane count(%d)\n", dsim->data_lane_cnt);

	dsim->lcd_info.data_lane = dsim->data_lane_cnt;

	if (dsim->lcd_info.mode == DECON_MIPI_COMMAND_MODE) {
		of_property_read_u32(node, "cmd_underrun_lp_ref",
			&dsim->lcd_info.cmd_underrun_lp_ref);
		dsim_info("cmd_underrun_lp_ref(%d)\n", dsim->lcd_info.cmd_underrun_lp_ref);
	} else {
		of_property_read_u32(node, "vt_compensation",
			&dsim->lcd_info.vt_compensation);
		dsim_info("vt_compensation(%d)\n", dsim->lcd_info.vt_compensation);

		of_property_read_u32(node, "clklane_onoff", &dsim->lcd_info.clklane_onoff);
		dsim_info("clklane onoff(%d)\n", dsim->lcd_info.clklane_onoff);
	}

	if (IS_ENABLED(CONFIG_FB_WINDOW_UPDATE)) {
		if (!of_property_read_u32_array(node, "update_min", res, 2)) {
			dsim->lcd_info.update_min_w = res[0];
			dsim->lcd_info.update_min_h = res[1];
			dsim_info("update_min_w(%d) update_min_h(%d) \n",
				dsim->lcd_info.update_min_w, dsim->lcd_info.update_min_h);
		} else { /* If values are not difined on DT, Set to full size */
			dsim->lcd_info.update_min_w = dsim->lcd_info.xres;
			dsim->lcd_info.update_min_h = dsim->lcd_info.yres;
			dsim_info("ERR: no update_min in DT!! update_min_w(%d) update_min_h(%d)\n",
			dsim->lcd_info.update_min_w, dsim->lcd_info.update_min_h);
		}
	}
}

static int dsim_parse_dt(struct dsim_device *dsim, struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->of_node)) {
		dsim_err("no device tree information\n");
		return -EINVAL;
	}

	dsim->id = of_alias_get_id(dev->of_node, "dsim");
	dsim_info("dsim(%d) probe start..\n", dsim->id);

#if !defined(PHY_FRAMEWORK_DIS)
/*	dsim->phy = devm_phy_get(dev, "dsim_dphy");
	if (IS_ERR_OR_NULL(dsim->phy)) {
		dsim_err("failed to get phy\n");
		return PTR_ERR(dsim->phy);
	}*/
#endif

	dsim->dev = dev;

	dsim_parse_lcd_info(dsim);

	return 0;
}
/*
static int dsim_init_resources(struct dsim_device *dsim, struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dsim_err("failed to get mem resource\n");
		return -ENOENT;
	}
//	dsim_info("res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);
//
//	dsim->res.regs = devm_ioremap_resource(dsim->dev, res);
//	if (!dsim->res.regs) {
//		dsim_err("failed to remap DSIM SFR region\n");
//		return -EINVAL;
//	}
//
//	dsim->res.ss_regs = dpu_get_sysreg_addr();
//	if (IS_ERR_OR_NULL(dsim->res.ss_regs)) {
//		dsim_err("failed to get sysreg addr\n");
//		return -EINVAL;
//	}

	return 0;
}
*/
static int dsim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct dsim_device *dsim = NULL;

	dsim = devm_kzalloc(dev, sizeof(struct dsim_device), GFP_KERNEL);
	if (!dsim) {
		dsim_err("failed to allocate dsim device.\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = dsim_parse_dt(dsim, dev);
	if (ret)
		goto err_dt;

	dsim_drvdata[dsim->id] = dsim;

	spin_lock_init(&dsim->slock);
	mutex_init(&dsim->cmd_lock);
//	init_completion(&dsim->ph_wr_comp);
//	init_completion(&dsim->rd_comp);

//	ret = dsim_init_resources(dsim, pdev);
//	if (ret)
//		goto err_dt;

//	dsim_init_subdev(dsim);
	platform_set_drvdata(pdev, dsim);
//	dsim_register_panel(dsim);
//	setup_timer(&dsim->cmd_timer, dsim_cmd_fail_detector,
//			(unsigned long)dsim);

#if defined(CONFIG_PM)
	pm_runtime_enable(dev);
#endif

	dsim_info("dsim%d driver(%s mode) has been probed.\n", dsim->id,
		dsim->lcd_info.mode == DECON_MIPI_COMMAND_MODE ? "cmd" : "video");

	return 0;

err_dt:
	kfree(dsim);
err:
	return ret;
}

static int dsim_remove(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&dsim->cmd_lock);
	dsim_info("dsim%d driver removed\n", dsim->id);

	return 0;
}

static const struct of_device_id dsim_of_match[] = {
	{ .compatible = "samsung,exynos7885-dsim" },
	{},
};
MODULE_DEVICE_TABLE(of, dsim_of_match);

static struct platform_driver dsim_driver __refdata = {
	.probe			= dsim_probe,
	.remove			= dsim_remove,
	.driver = {
		.name		= DSIM_MODULE_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(dsim_of_match),
		.suppress_bind_attrs = true,
	}
};

static int __init dsim_init(void)
{
	int ret = platform_driver_register(&dsim_driver);

	if (ret)
		pr_err("dsim driver register failed\n");

	return ret;
}
module_init(dsim_init);

static void __exit dsim_exit(void)
{
	platform_driver_unregister(&dsim_driver);
}

module_exit(dsim_exit);
MODULE_AUTHOR("Yeongran Shin <yr613.shin@samsung.com>");
MODULE_AUTHOR("SeungBeom Park <sb1.park@samsung.com>");
MODULE_DESCRIPTION("Samusung EXYNOS DSIM driver");
MODULE_LICENSE("GPL");
