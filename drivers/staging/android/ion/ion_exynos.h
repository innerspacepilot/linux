/*
 * drivers/staging/android/ion/ion_exynos.h
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ION_EXYNOS_H_
#define _ION_EXYNOS_H_

#include <linux/dma-direction.h>
struct dma_buf;
struct dma_buf_attachment;

#ifdef CONFIG_ION_EXYNOS
struct sg_table *ion_exynos_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction);
void ion_exynos_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction);
#else
static struct sg_table *ion_exynos_map_dma_buf(struct dma_buf_attachment *att,
					       enum dma_data_direction dir)
{
	return ERR_PTR(-ENODEV);
}

#define ion_exynos_unmap_dma_buf(attachment, table, direction) do { } while (0)
#endif

struct ion_heap *ion_get_heap_by_name(const char *heap_name);
struct dma_buf *__ion_alloc(size_t len, unsigned int heap_id_mask,
			    unsigned int flags);
#endif /* _ION_EXYNOS_H_ */
