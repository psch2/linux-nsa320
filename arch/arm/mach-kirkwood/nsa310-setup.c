/*
 * arch/arm/mach-kirkwood/nsa310-setup.c
 *
 * ZyXEL NSA310 1-Bay Power Media Server Setup
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/ata_platform.h>
#include <linux/mv643xx_eth.h>
#include <linux/ethtool.h>  /* for SPEED_1000, DUPLEX_FULL */
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/leds.h>
#include <linux/input.h>
#include <linux/nsa3xx-hwmon.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/kirkwood.h>
#include "common.h"
#include "mpp.h"

/*
 * FIXME: /dev/mtdblock6 and /dev/mtdblock8 only:
 *
 * / # cat /dev/mtdblock6 > /dev/null
 * [   42.067831] uncorrectable error :
 * [   42.071125] uncorrectable error :
 * [   42.074569] end_request: I/O error, dev mtdblock6, sector 0
 * [   42.080346] Buffer I/O error on device mtdblock6, logical block 0
 * [   42.086535] uncorrectable error :
 * [   42.089777] uncorrectable error :
 * [   42.093209] end_request: I/O error, dev mtdblock6, sector 8
 * [   42.098988] Buffer I/O error on device mtdblock6, logical block 1
 * [   42.105172] uncorrectable error :
 * [   42.108413] uncorrectable error :
 * [   42.111833] end_request: I/O error, dev mtdblock6, sector 16
 * [   42.117701] Buffer I/O error on device mtdblock6, logical block 2
 * [   42.123964] uncorrectable error :
 * [   42.127204] uncorrectable error :
 * [   42.130625] end_request: I/O error, dev mtdblock6, sector 24
 * [   42.136493] Buffer I/O error on device mtdblock6, logical block 3
 * [   42.142724] uncorrectable error :
 * [   42.145960] uncorrectable error :
 * [   42.149381] end_request: I/O error, dev mtdblock6, sector 0
 * [   42.155163] Buffer I/O error on device mtdblock6, logical block 0
 * cat: read error: Input/output error
 */

static struct mtd_partition nsa310_nand_parts[] = {
	{
		.name = "uboot",
		.offset = 0,
		.size = SZ_1M,
		.mask_flags = MTD_WRITEABLE
	}, {
		.name = "uboot_env",
		.offset = MTDPART_OFS_NXTBLK,
		.size = SZ_512K
	}, {
		.name = "key_store",
		.offset = MTDPART_OFS_NXTBLK,
		.size = SZ_512K
	}, {
		.name = "info",
		.offset = MTDPART_OFS_NXTBLK,
		.size = SZ_512K
	}, {
		.name = "etc",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 10 * SZ_1M
	}, {
		.name = "kernel_1",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 10 * SZ_1M
	}, {
		.name = "rootfs1",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 48 * SZ_1M - SZ_256K
	}, {
		.name = "kernel_2",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 10 * SZ_1M
	}, {
		.name = "rootfs2",
		.offset = MTDPART_OFS_NXTBLK,
		.size = 48 * SZ_1M - SZ_256K
	},
};

static struct i2c_board_info __initdata nsa310_i2c_rtc = {
	I2C_BOARD_INFO("pcf8563", 0x51),
};

static struct mv643xx_eth_platform_data nsa310_ge00_data = {
	.phy_addr	= MV643XX_ETH_PHY_ADDR(0),
	/* FIXME: autonegotiation */
	.speed		= SPEED_1000,
	.duplex		= DUPLEX_FULL,
};

static struct mv_sata_platform_data nsa310_sata_data = {
	.n_ports	= 2,
};

static struct gpio_keys_button nsa310_button_pins[] = {
	{
		.code		= KEY_RESTART,
		.gpio		= 36,
		.desc		= "Reset",
		.active_low	= 1,
	}, {
		.code		= KEY_COPY,
		.gpio		= 37,
		.desc		= "Copy",
		.active_low	= 1,
	}, {
		.code		= KEY_POWER,
		.gpio		= 46,
		.desc		= "Power",
		.active_low	= 0,
	},
};

static struct gpio_keys_platform_data nsa310_button_data = {
	.buttons	= nsa310_button_pins,
	.nbuttons	= ARRAY_SIZE(nsa310_button_pins),
};

static struct platform_device nsa310_buttons = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &nsa310_button_data,
	},
};

static struct gpio_led nsa310_led_pins[] = {
	{
		.name			= "nsa310:green:hdd2",
		.default_trigger	= "default-off",
		.gpio			= 12,
		.active_low		= 0,
	}, {
		.name			= "nsa310:red:hdd2",
		.default_trigger	= "default-off",
		.gpio			= 13,
		.active_low		= 0,
	}, {
		.name			= "nsa310:green:usb",
		.default_trigger	= "default-off",
		.gpio			= 15,
		.active_low		= 0,
	}, {
		.name			= "nsa310:green:sys",
		.default_trigger	= "default-off",
		.gpio			= 28,
		.active_low		= 0,
	}, {
		.name			= "nsa310:orange:sys",
		.default_trigger	= "default-on",
		.gpio			= 29,
		.active_low		= 0,
	}, {
		.name			= "nsa310:green:copy",
		.default_trigger	= "default-off",
		.gpio			= 39,
		.active_low		= 0,
	}, {
		.name			= "nsa310:red:copy",
		.default_trigger	= "default-off",
		.gpio			= 40,
		.active_low		= 0,
	}, {
		.name			= "nsa310:green:hdd1",
		.default_trigger	= "default-off",
		.gpio			= 41,
		.active_low		= 0,
	}, {
		.name			= "nsa310:red:hdd1",
		.default_trigger	= "default-off",
		.gpio			= 42,
		.active_low		= 0,
	},
};

static struct gpio_led_platform_data nsa310_led_data = {
	.leds		= nsa310_led_pins,
	.num_leds	= ARRAY_SIZE(nsa310_led_pins),
};

static struct platform_device nsa310_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &nsa310_led_data,
	},
};

static struct nsa3xx_hwmon_platform_data nsa310_hwmon_data = {
	/* GPIOs connected to Holtek HT46R065 MCU */
	.act_pin  = 17,
	.clk_pin  = 16,
	.data_pin = 14,
};

static struct platform_device nsa310_hwmon = {
	.name		= "nsa3xx-hwmon",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &nsa310_hwmon_data,
	},
};

static unsigned int nsa310_mpp_config[] __initdata = {
	MPP0_NF_IO2,
	MPP1_NF_IO3,
	MPP2_NF_IO4,
	MPP3_NF_IO5,
	MPP4_NF_IO6,
	MPP5_NF_IO7,
	MPP6_SYSRST_OUTn,
	MPP7_GPO,
	MPP8_TW0_SDA,	/* PCF8563 RTC chip   */
	MPP9_TW0_SCK,	/* connected to TWSI  */
	MPP10_UART0_TXD,
	MPP11_UART0_RXD,
	MPP12_GPO,	/* HDD2 LED (green)   */
	MPP13_GPIO,	/* HDD2 LED (red)     */
	MPP14_GPIO,	/* MCU DATA pin (in)  */
	MPP15_GPIO,	/* USB LED (green)    */
	MPP16_GPIO,	/* MCU CLK pin (out)  */
	MPP17_GPIO,	/* MCU ACT pin (out)  */
	MPP18_NF_IO0,
	MPP19_NF_IO1,
	MPP20_GPIO,
	MPP21_GPIO,	/* USB power          */
	MPP22_GPIO,
	MPP23_GPIO,
	MPP24_GPIO,
	MPP25_GPIO,
	MPP26_GPIO,
	MPP27_GPIO,
	MPP28_GPIO,	/* SYS LED (green)    */
	MPP29_GPIO,	/* SYS LED (orange)   */
	MPP30_GPIO,
	MPP31_GPIO,
	MPP32_GPIO,
	MPP33_GPO,
	MPP34_GPIO,
	MPP35_GPIO,
	MPP36_GPIO,	/* reset button       */
	MPP37_GPIO,	/* copy button        */
	MPP38_GPIO,	/* VID B0             */
	MPP39_GPIO,	/* COPY LED (green)   */
	MPP40_GPIO,	/* COPY LED (red)     */
	MPP41_GPIO,	/* HDD1 LED (green)   */
	MPP42_GPIO,	/* HDD1 LED (red)     */
	MPP43_GPIO,	/* HTP pin            */
	MPP44_GPIO,	/* buzzer             */
	MPP45_GPIO,	/* VID B1             */
	MPP46_GPIO,	/* power button       */
	MPP47_GPIO,	/* power resume data  */
	MPP48_GPIO,	/* power off          */
	MPP49_GPIO,	/* power resume clock */
	0
};

#define NSA310_GPIO_USB_POWER	21
#define NSA310_GPIO_POWER_OFF	48

static void nsa310_power_off(void)
{
	gpio_set_value(NSA310_GPIO_POWER_OFF, 1);
}

static int __initdata usb_power = 1; /* default "on" */

static int __init nsa310_usb_power(char *str)
{
	usb_power = strncmp(str, "off", 3) ? 1 : 0;
	return 1;
}
/* Parse boot_command_line string nsa310_usb_power=on|off */
__setup("nsa310_usb_power=", nsa310_usb_power);

static void __init nsa310_init(void)
{
	/*
	 * Basic setup. Needs to be called early.
	 */
	kirkwood_init();
	kirkwood_mpp_conf(nsa310_mpp_config);

	kirkwood_uart0_init();
	kirkwood_nand_init(ARRAY_AND_SIZE(nsa310_nand_parts), 25);

	kirkwood_i2c_init();
	i2c_register_board_info(0, &nsa310_i2c_rtc, 1);

	if (gpio_request(NSA310_GPIO_USB_POWER, "USB Power Enable") ||
	    gpio_direction_output(NSA310_GPIO_USB_POWER, usb_power))
		pr_err("nsa310: failed to configure USB power enable GPIO)\n");
	gpio_free(NSA310_GPIO_USB_POWER);

	kirkwood_ehci_init();
	kirkwood_ge00_init(&nsa310_ge00_data);
	kirkwood_sata_init(&nsa310_sata_data);
	platform_device_register(&nsa310_leds);
	platform_device_register(&nsa310_buttons);
	platform_device_register(&nsa310_hwmon);

	if (gpio_request(NSA310_GPIO_POWER_OFF, "power-off") ||
	    gpio_direction_output(NSA310_GPIO_POWER_OFF, 0))
		pr_err("nsa310: failed to configure power-off GPIO\n");
	else
		pm_power_off = nsa310_power_off;
}

MACHINE_START(NSA310, "ZyXEL NSA310 1-Bay Power Media Server")
	/* Maintainer: Peter Oostewechel <peter_oostewechel@hotmail.com> */
	.atag_offset	= 0x100,
	.init_machine	= nsa310_init,
	.map_io		= kirkwood_map_io,
	.init_early	= kirkwood_init_early,
	.init_irq	= kirkwood_init_irq,
	.timer		= &kirkwood_timer,
	.restart	= kirkwood_restart,
MACHINE_END
