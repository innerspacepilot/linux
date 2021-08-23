/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS8 SoC series DPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/exynos_iovmm.h>
#include <linux/videodev2_exynos_media.h>

#include "dpp.h"
#include "decon.h"

int dpp_log_level = 6;
struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static void dpp_get_params(struct dpp_device *dpp, struct dpp_params_info *p)
{
	struct decon_win_config *config = dpp->config;

	p->addr[0] = config->dpp_parm.addr[0];
	p->addr[1] = config->dpp_parm.addr[1];
	p->addr[2] = config->dpp_parm.addr[2];
}

static void dpp_set_config(struct dpp_device *dpp)
{
	struct dpp_params_info params;

	mutex_lock(&dpp->lock);

	/* parameters from decon driver are translated for dpp driver */
	dpp_get_params(dpp, &params);

       if (dpp->state == DPP_STATE_OFF || dpp->state == DPP_STATE_BOOT) {
#if defined(CONFIG_PM)
               pm_runtime_get_sync(dpp->dev);
#endif
       }

	/* set all parameters to dpp hw */
	dpp_reg_configure_params(dpp->id, &params);

	dpp->state = DPP_STATE_ON;

	mutex_unlock(&dpp->lock);
	dpp_dbg("dpp%d is finished\n", dpp->id);
}

int dpp_set_config_ext(struct dpp_device *dpp, struct decon_win_config *arg)
{
	int ret = 0;
	dpp->config = (struct decon_win_config *)arg;
	dpp_set_config(dpp);
	if (ret)
		dpp_err("failed to configure dpp%d\n", dpp->id);
	return ret;
}
EXPORT_SYMBOL(dpp_set_config_ext);

static void dpp_parse_dt(struct dpp_device *dpp, struct device *dev)
{
	dpp->id = of_alias_get_id(dev->of_node, "dpp");
	dpp_info("dpp(%d) probe start..\n", dpp->id);

	dpp->dev = dev;
}

static int dpp_init_resources(struct dpp_device *dpp, struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dpp_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dpp_info("res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dpp->res.regs = devm_ioremap_resource(dpp->dev, res);
	if (!dpp->res.regs) {
		dpp_err("failed to remap DPP SFR region\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dpp_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dpp_info("dma res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dpp->res.dma_regs = devm_ioremap_resource(dpp->dev, res);
	if (!dpp->res.dma_regs) {
		dpp_err("failed to remap DPU_DMA SFR region\n");
		return -EINVAL;
	}

	if (dpp->id == IDMA_G0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!res) {
			dpp_err("failed to get mem resource\n");
			return -ENOENT;
		}
		dpp_info("dma common res: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);
		dpp->res.dma_com_regs = devm_ioremap_resource(dpp->dev, res);
		if (!dpp->res.dma_com_regs) {
			dpp_err("failed to remap DPU_DMA COMMON SFR region\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int dpp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dpp_device *dpp;
	int ret = 0;

	dpp = devm_kzalloc(dev, sizeof(*dpp), GFP_KERNEL);
	if (!dpp) {
		dpp_err("failed to allocate dpp device.\n");
		ret = -ENOMEM;
		goto err;
	}
	dpp_parse_dt(dpp, dev);

	dpp_drvdata[dpp->id] = dpp;

	spin_lock_init(&dpp->slock);
	spin_lock_init(&dpp->dma_slock);
	mutex_init(&dpp->lock);
	init_waitqueue_head(&dpp->framedone_wq);

	ret = dpp_init_resources(dpp, pdev);
	if (ret)
		goto err_clk;

	platform_set_drvdata(pdev, dpp);

	pm_runtime_enable(dev);

	ret = iovmm_activate(dev);
	if (ret) {
		dpp_err("failed to activate iovmm\n");
		goto err_clk;
	}

	dpp->state = DPP_STATE_OFF;

	dpp_info("dpp%d is probed successfully\n", dpp->id);

	return 0;

err_clk:
	kfree(dpp);
err:
	return ret;
}

static int dpp_remove(struct platform_device *pdev)
{
	struct dpp_device *dpp = platform_get_drvdata(pdev);

	iovmm_deactivate(dpp->dev);
	dpp_dbg("%s driver unloaded\n", pdev->name);

	return 0;
}

static const struct of_device_id dpp_of_match[] = {
	{ .compatible = "samsung,exynos7885-dpp" },
	{},
};
MODULE_DEVICE_TABLE(of, dpp_of_match);

static struct platform_driver dpp_driver __refdata = {
	.probe		= dpp_probe,
	.remove		= dpp_remove,
	.driver = {
		.name	= DPP_MODULE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dpp_of_match),
		.suppress_bind_attrs = true,
	}
};

static int dpp_register(void)
{
	return platform_driver_register(&dpp_driver);
}

static void dpp_unregister(void)
{
	return platform_driver_unregister(&dpp_driver);
}

module_init(dpp_register);
module_exit(dpp_unregister);

MODULE_AUTHOR("Jaehoe Yang <jaehoe.yang@samsung.com>");
MODULE_AUTHOR("Minho Kim <m8891.kim@samsung.com>");
MODULE_AUTHOR("SeungBeom Park <sb1.park@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS DPP driver");
MODULE_LICENSE("GPL");
