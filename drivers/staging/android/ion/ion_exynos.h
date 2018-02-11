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

struct dma_buf;

struct ion_heap *ion_get_heap_by_name(const char *heap_name);
struct dma_buf *__ion_alloc(size_t len, unsigned int heap_id_mask,
			    unsigned int flags);
#endif /* _ION_EXYNOS_H_ */
