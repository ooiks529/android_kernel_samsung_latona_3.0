/*
 * Copyright (C) 2009-2010 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 * Felipe Balbi <balbi@ti.com>
 *
 * Modified from mach-omap2/board-zoom.c for Samsung Latona board
 *
 * Aditya Patange aka "Adi_Pat" <adithemagnficent@gmail.com> 
 * Mark "Hill Beast" Kennard <komcomputers@gmail.com>
 * Phinitnan "Crackerizer" Chanasabaeng <phinitnan_c@xtony.us>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/memblock.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/wl12xx.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/sizes.h>
#include <plat/common.h>
#include <plat/board.h>
#include <plat/usb.h>

#include <mach/board-latona.h>

#include "board-flash.h"
#include "mux.h"
#include "sdram-qimonda-hyb18m512160af-6.h"
#include "omap_ion.h"

#define LATONA_EHCI_RESET_GPIO		64
#define LATONA_McBSP3_BT_GPIO            164
#define LATONA_BT_RESET_GPIO             109
#define LATONA_WIFI_PMENA_GPIO		157
#define LATONA_WIFI_IRQ_GPIO		162

#define WILINK_UART_DEV_NAME            "/dev/ttyO1"

#ifdef CONFIG_OMAP_MUX
extern struct omap_board_mux *latona_board_mux_ptr;
extern struct omap_board_mux *latona_board_wk_mux_ptr;
#else
#define latona_board_mux_ptr		NULL
#define latona_board_wk_mux_ptr		NULL
#endif

static void __init latona_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(hyb18m512160af6_sdrc_params,
					  hyb18m512160af6_sdrc_params);
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/* WLAN IRQ - GPIO 162 */
	OMAP3_MUX(MCBSP1_CLKX, OMAP_MUX_MODE4 | OMAP_PIN_INPUT),
	/* WLAN POWER ENABLE - GPIO 101 */
	OMAP3_MUX(CAM_D2, OMAP_MUX_MODE4 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC3 CMD */
	OMAP3_MUX(MCSPI1_CS1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 CLK */
	OMAP3_MUX(ETK_CLK, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 DAT[0-3] */
	OMAP3_MUX(ETK_D3, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D4, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D5, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D6, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[2]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset		= true,
	.reset_gpio_port[0]	= -EINVAL,
	.reset_gpio_port[1]	= LATONA_EHCI_RESET_GPIO,
	.reset_gpio_port[2]	= -EINVAL,
};

static int plat_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* TODO: wait for HCI-LL sleep */
	return 0;
}
static int plat_kim_resume(struct platform_device *pdev)
{
	return 0;
}

/* wl127x BT, FM, GPS connectivity chip */
struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = 109,
	.dev_name = WILINK_UART_DEV_NAME,
	.flow_cntrl = 1,
	.baud_rate = 3686400,
	.suspend = plat_kim_suspend,
	.resume = plat_kim_resume,
};
static struct platform_device wl127x_device = {
	.name           = "kim",
	.id             = -1,
	.dev.platform_data = &wilink_pdata,
};
static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device *latona_devices[] __initdata = {
	&wl127x_device,
	&btwilink_device,
};

/* Fix to prevent VIO leakage on wl127x */
static int wl127x_vio_leakage_fix(void)
{
	int ret = 0;

	pr_info(" wl127x_vio_leakage_fix\n");

	ret = gpio_request(LATONA_BT_RESET_GPIO, "wl127x_bten");
	if (ret < 0) {
		pr_err("wl127x_bten gpio_%d request fail",
			LATONA_BT_RESET_GPIO);
		goto fail;
	}

	gpio_direction_output(LATONA_BT_RESET_GPIO, 1);
	mdelay(10);
	gpio_direction_output(LATONA_BT_RESET_GPIO, 0);
	udelay(64);

	gpio_free(LATONA_BT_RESET_GPIO);
fail:
	return ret;
}

static void config_wlan_mux(void)
{
	/* WLAN PW_EN and IRQ */
	omap_mux_init_gpio(LATONA_WIFI_PMENA_GPIO, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(LATONA_WIFI_IRQ_GPIO, OMAP_PIN_INPUT |
				OMAP_PIN_OFF_WAKEUPENABLE);

	/* MMC3 */
	omap_mux_init_signal("etk_clk.sdmmc3_clk", OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("mcspi1_cs1.sdmmc3_cmd", OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("etk_d4.sdmmc3_dat0", OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("etk_d5.sdmmc3_dat1", OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("etk_d6.sdmmc3_dat2", OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("etk_d3.sdmmc3_dat3", OMAP_PIN_INPUT_PULLUP);
}

static struct wl12xx_platform_data latona_wlan_data __initdata = {
	.irq = OMAP_GPIO_IRQ(LATONA_WIFI_IRQ_GPIO),
	.board_ref_clock = WL12XX_REFCLOCK_26,
	.board_tcxo_clock = WL12XX_TCXOCLOCK_26,
};

static void latona_wifi_init(void)
{
	config_wlan_mux();
	if (wl12xx_set_platform_data(&latona_wlan_data))
		pr_err("Error setting wl12xx data\n");
}

static void __init latona_init(void)
{
	omap3_mux_init(latona_board_mux_ptr, OMAP_PACKAGE_CBP);
	latona_mux_init_gpio_out();
	latona_mux_set_wakeup_gpio();

	omap_mux_init_gpio(LATONA_EHCI_RESET_GPIO, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(LATONA_McBSP3_BT_GPIO, OMAP_PIN_OUTPUT);
	usbhs_init(&usbhs_bdata);

	latona_wifi_init();
	latona_debugboard_init();
	latona_peripherals_init();
	latona_display_init();
	omap_register_ion();
	/* Added to register latona devices */
	platform_add_devices(latona_devices, ARRAY_SIZE(latona_devices));
	wl127x_vio_leakage_fix();
}

static void __init latona_reserve(void)
{
	/* do the static reservations first */
	memblock_remove(OMAP3_PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);

#ifdef CONFIG_ION_OMAP
	omap_ion_init();
#endif
	omap_reserve();
}

static void __init latona_fixup(struct machine_desc *desc,
				    struct tag *tags, char **cmdline,
				    struct meminfo *mi)
{
	/* Fix RAM Settings */ 
	printk("Starting %s \n", __func__);
	mi->nr_banks = 2;
	mi->bank[0].start = 0x80000000;
	mi->bank[0].size = SZ_256M;
	mi->bank[1].start = 0x90000000;
	mi->bank[1].size = SZ_256M;
}

MACHINE_START(LATONA, "LATONA")
	.boot_params	= 0x80000100,
	.fixup          = latona_fixup,
	.reserve	= latona_reserve,
	.map_io		= omap3_map_io,
	.init_early	= latona_init_early,
	.init_irq	= omap_init_irq,
	.init_machine	= latona_init,
	.timer		= &omap_timer,
MACHINE_END
