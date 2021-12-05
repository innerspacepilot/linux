// SPDX-License-Identifier: GPL-2.0+
/*
 * lmp91000.c - Support for Texas Instruments digital potentiostats
 *
 * Copyright (C) 2016, 2018
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 *
 * TODO: bias voltage + polarity control, and multiple chip support
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mod_devicetable.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/consumer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

static int s2mu004_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	i2c_smbus_write_byte_data(client, 0xCA, 0x48);
	i2c_smbus_write_byte_data(client, 0xC7, 0x12);

	return 0;
}

static int s2mu004_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id s2mu004_of_match[] = {
	{ .compatible = "samsung,s2mu004", },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mu004_of_match);

static const struct i2c_device_id s2mu004_id[] = {
	{ "s2mu004", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, s2mu004_id);

static struct i2c_driver s2mu004_driver = {
	.driver = {
		.name = "s2mu004",
		.of_match_table = s2mu004_of_match,
	},
	.probe = s2mu004_probe,
	.remove = s2mu004_remove,
	.id_table = s2mu004_id,
};
module_i2c_driver(s2mu004_driver);

MODULE_AUTHOR("Matt Ranostay <matt.ranostay@konsulko.com>");
MODULE_DESCRIPTION("LMP91000 digital potentiostat");
MODULE_LICENSE("GPL");

