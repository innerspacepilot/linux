#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPP) += dpp.o
dpp-y := dpp_drv.o
obj-$(CONFIG_EXYNOS_MIPI_DSIM) += dsim.o
dsim-y += dsim_drv.o
obj-y += dpp_reg.o decon_reg.o
obj-$(CONFIG_EXYNOS_DECON_7885) += decon.o
decon-y += decon_core.o decon_dsi.o helper.o

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui
