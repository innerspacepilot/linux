/* linux/drivers/video/exynos/decon/dpp/dpp_regs.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series G-scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

//#include <linux/io.h>
//#include <linux/delay.h>
//#include <linux/ktime.h>
#include "dpp.h"
//#include "dpp_coef.h"

void dma_reg_set_in_base_addr(u32 id, u32 addr_y, u32 addr_c)
{
	if (id == ODMA_WB) {
		dma_write(id, ODMA_IN_BASE_ADDR_Y, addr_y);
		dma_write(id, ODMA_IN_BASE_ADDR_C, addr_c);
	} else {
		dma_write(id, IDMA_IN_BASE_ADDR_Y, addr_y);
		dma_write(id, IDMA_IN_BASE_ADDR_C, addr_c);
	}
}

void dpp_reg_set_buf_addr(u32 id, struct dpp_params_info *p)
{
	dpp_dbg("y : %llu, cb : %llu, cr : %llu\n",
			p->addr[0], p->addr[1], p->addr[2]);

	/* For AFBC stream, BASE_ADDR_C must be same with BASE_ADDR_Y */
	if (p->is_comp == 0)
		dma_reg_set_in_base_addr(id, p->addr[0], p->addr[1]);
	else
		dma_reg_set_in_base_addr(id, p->addr[0], p->addr[0]);
}

void dpp_reg_configure_params(u32 id, struct dpp_params_info *p)
{
	dpp_reg_set_buf_addr(id, p);
}
