/*
 * drivers/hwmon/nsa3xx-hwmon.c
 *
 * ZyXEL NSA3xx Media Servers
 * hardware monitoring
 *
 * Copyright (C) 2012 Peter Schildmann <linux@schildmann.info>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/nsa3xx-hwmon.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/delay.h>

#define MAGIC_NUMBER 0x55

struct nsa3xx_hwmon {
	struct platform_device	*pdev;
	struct device		*classdev;
	struct mutex		update_lock;	/* lock GPIO operations */
	unsigned long		last_updated;	/* jiffies */
	unsigned long		mcu_data;
};

enum nsa3xx_inputs {
	NSA3XX_FAN = 1,
	NSA3XX_TEMP = 0,
};

static const char *nsa3xx_input_names[] = {
	[NSA3XX_FAN] = "Chassis Fan",
	[NSA3XX_TEMP] = "System Temperature",
};

static unsigned long nsa3xx_hwmon_update(struct device *dev)
{
	int i;
	unsigned long mcu_data;
	struct nsa3xx_hwmon *hwmon = dev_get_drvdata(dev);
	struct nsa3xx_hwmon_platform_data *pdata = hwmon->pdev->dev.platform_data;

	mutex_lock(&hwmon->update_lock);

	mcu_data = hwmon->mcu_data;

	if (time_after(jiffies, hwmon->last_updated + (3 * HZ)) || mcu_data == 0) {
		dev_dbg(dev, "Reading MCU data\n");

		gpio_set_value(pdata->act_pin, 0);
		msleep(100);

		for (i = 31; i >= 0; i--) {
			gpio_set_value(pdata->clk_pin, 0);
			udelay(100);

			gpio_set_value(pdata->clk_pin, 1);
			udelay(100);

			mcu_data |= gpio_get_value(pdata->data_pin) ? (1 << i) : 0;
		}

		gpio_set_value(pdata->act_pin, 1);

		if ((mcu_data & 0xff000000) != (MAGIC_NUMBER << 24)) {
			dev_err(dev, "Failed to read MCU data\n");
			mcu_data = 0;
		}

		hwmon->mcu_data = mcu_data;
		hwmon->last_updated = jiffies;
	}

	mutex_unlock(&hwmon->update_lock);

	return mcu_data;
}

static ssize_t show_name(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "nsa3xx\n");
}

static ssize_t show_label(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;
	return sprintf(buf, "%s\n", nsa3xx_input_names[channel]);
}

static ssize_t show_value(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int channel = to_sensor_dev_attr(attr)->index;
	unsigned long mcu_data = nsa3xx_hwmon_update(dev);
	unsigned long value = 0;
	switch(channel) {
	case NSA3XX_TEMP:
		value = (mcu_data & 0xffff) * 100;
		break;
	case NSA3XX_FAN:
		value = ((mcu_data & 0xff0000) >> 16) * 100;
		break;
	}
	return sprintf(buf, "%lu\n", value);
}

static DEVICE_ATTR(name, S_IRUGO, show_name, NULL);
static SENSOR_DEVICE_ATTR(temp1_label, S_IRUGO, show_label, NULL, NSA3XX_TEMP);
static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_value, NULL, NSA3XX_TEMP);
static SENSOR_DEVICE_ATTR(fan1_label, S_IRUGO, show_label, NULL, NSA3XX_FAN);
static SENSOR_DEVICE_ATTR(fan1_input, S_IRUGO, show_value, NULL, NSA3XX_FAN);

static struct attribute *nsa3xx_attributes[] = {
	&dev_attr_name.attr,
	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_label.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	NULL
};

static const struct attribute_group nsa3xx_attr_group = {
	.attrs	= nsa3xx_attributes,
};

static int nsa3xx_hwmon_request_gpios(struct nsa3xx_hwmon_platform_data *pdata)
{
	int ret;

	if ((ret = gpio_request(pdata->act_pin, "act pin")))
		return ret;

	if ((ret = gpio_request(pdata->clk_pin, "clk pin")))
		return ret;

	if ((ret = gpio_request(pdata->data_pin, "data pin")))
		return ret;

	if ((ret = gpio_direction_output(pdata->act_pin, 1)))
		return ret;

	if ((ret = gpio_direction_output(pdata->clk_pin, 1)))
		return ret;

	if ((ret = gpio_direction_input(pdata->data_pin)))
		return ret;

	return 0;
}

static void nsa3xx_hwmon_free_gpios(struct nsa3xx_hwmon_platform_data *pdata)
{
	gpio_free(pdata->act_pin);
	gpio_free(pdata->clk_pin);
	gpio_free(pdata->data_pin);
}

static int __devinit nsa3xx_hwmon_probe(struct platform_device *pdev)
{
	int ret;
	struct nsa3xx_hwmon *hwmon;
	struct nsa3xx_hwmon_platform_data *pdata = pdev->dev.platform_data;

	hwmon = kzalloc(sizeof(struct nsa3xx_hwmon), GFP_KERNEL);
	if (!hwmon)
		return -ENOMEM;

	platform_set_drvdata(pdev, hwmon);
	hwmon->pdev = pdev;
	hwmon->mcu_data = 0;
	mutex_init(&hwmon->update_lock);

	ret = sysfs_create_group(&pdev->dev.kobj, &nsa3xx_attr_group);
	if (ret)
		goto err;

	hwmon->classdev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(hwmon->classdev)) {
		ret = PTR_ERR(hwmon->classdev);
		goto err_sysfs;
	}

	ret = nsa3xx_hwmon_request_gpios(pdata);
	if (ret)
		goto err_free_gpio;

	dev_info(&pdev->dev, "initialized\n");

	return 0;

err_free_gpio:
	nsa3xx_hwmon_free_gpios(pdata);
	hwmon_device_unregister(hwmon->classdev);
err_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &nsa3xx_attr_group);
err:
	platform_set_drvdata(pdev, NULL);
	kfree(hwmon);
	return ret;
}

static int __devexit nsa3xx_hwmon_remove(struct platform_device *pdev)
{
	struct nsa3xx_hwmon *hwmon = platform_get_drvdata(pdev);

	nsa3xx_hwmon_free_gpios(pdev->dev.platform_data);
	hwmon_device_unregister(hwmon->classdev);
	sysfs_remove_group(&pdev->dev.kobj, &nsa3xx_attr_group);
	platform_set_drvdata(pdev, NULL);
	kfree(hwmon);

	return 0;
}

static struct platform_driver nsa3xx_hwmon_driver = {
	.probe = nsa3xx_hwmon_probe,
	.remove = __devexit_p(nsa3xx_hwmon_remove),
	.driver = {
		.name = "nsa3xx-hwmon",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(nsa3xx_hwmon_driver);

MODULE_AUTHOR("Peter Schildmann <linux@schildmann.info>");
MODULE_DESCRIPTION("NSA3XX Hardware Monitoring");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:nsa3xx-hwmon");
