/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define pr_fmt(fmt)	"%s " fmt,  __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/raid/pq.h>
#include <linux/extcon.h>
#include <linux/proc_fs.h>
#include "nu2115.h"
#include <sqc_common.h>
#include <vendor/common/zte_misc.h>
#include "sqc_netlink.h"

static int cp1_init_finished = 0;
static int cp2_init_finished = 0;
static int nu2115_init_finish_flag = NU2115_NOT_INIT;
static int nu2115_int_notify_enable_flag = NU2115_DISABLE_INT_NOTIFY;

#define MSG_LEN                      (2)


#define nu_err(fmt, ...)								\
do {											\
	if (chip->hw_mode == NU2115_MASTER)						\
		pr_err("[ZTE_LDD_CHARGE][nu2115-MASTER]:" fmt, ##__VA_ARGS__);	\
	else if (chip->hw_mode == NU2115_SLAVE)					\
		pr_err("[ZTE_LDD_CHARGE][nu2115-SLAVE]:" fmt, ##__VA_ARGS__);	\
	else										\
		pr_err("[ZTE_LDD_CHARGE][nu2115-STANDALONE]:" fmt, ##__VA_ARGS__);\
} while (0);

#define nu_info(fmt, ...)								\
do {											\
	if (chip->hw_mode == NU2115_MASTER)						\
		pr_info("[ZTE_LDD_CHARGE][nu2115-MASTER]:" fmt, ##__VA_ARGS__);	\
	else if (chip->hw_mode == NU2115_SLAVE)					\
		pr_info("[ZTE_LDD_CHARGE][nu2115-SLAVE]:" fmt, ##__VA_ARGS__);	\
	else										\
		pr_info("[ZTE_LDD_CHARGE][nu2115-STANDALONE]:" fmt, ##__VA_ARGS__);\
} while (0);


#define nu_dbg(fmt, ...)								\
do {											\
	if (chip->hw_mode == NU2115_MASTER)						\
		pr_debug("[ZTE_LDD_CHARGE][nu2115-MASTER]:" fmt, ##__VA_ARGS__);	\
	else if (chip->hw_mode == NU2115_SLAVE)					\
		pr_debug("[ZTE_LDD_CHARGE][nu2115-SLAVE]:" fmt, ##__VA_ARGS__);	\
	else										\
		pr_debug("[ZTE_LDD_CHARGE][nu2115-STANDALONE]:" fmt, ##__VA_ARGS__);\
} while (0);


static const struct of_device_id nu2115_of_match[] = {
	{
		.compatible = "nu2115-standalone",
		.data = (void *)NU2115_STDALONE,
	},
	{
		.compatible = "nu2115-master",
		.data = (void *)NU2115_MASTER,
	},
	{
		.compatible = "nu2115-slave",
		.data = (void *)NU2115_SLAVE,
	},
	{},
};

static const struct i2c_device_id nu2115_i2c_id[] = {
	{"nu2115-standalone", NU2115_STDALONE},
	{"nu2115-master", NU2115_MASTER},
	{"nu2115-slave", NU2115_SLAVE},
	{},
};

static bool nu2115_update_status_flag(struct nu2115_device *chip, int flag, int status_code)
{
	int chg_id = SQC_NOTIFY_CHG_CP1;

	if (chip->hw_mode == NU2115_SLAVE)
		chg_id = SQC_NOTIFY_CHG_CP2;

	if (flag) {
		sqc_notify_daemon_changed(chg_id, status_code, 1);
		nu_info("%s status_code %d set to 1\n", __func__, status_code);
		return true;
	}

	return false;
}

static int nu2115_write_block(struct nu2115_device *chip,
	u8 *value, u8 reg, unsigned int num_bytes)
{
	struct i2c_msg msg[1];
	int ret = 0;

	if (chip == NULL || value == NULL) {
		nu_err("chip is null or value is null\n");
		return -EIO;
	}

	if (chip->chip_already_init == 0) {
		nu_err("chip not init\n");
		return -EIO;
	}

	*value = reg;

	msg[0].addr = chip->client->addr;
	msg[0].flags = 0;
	msg[0].buf = value;
	msg[0].len = num_bytes + 1;

	ret = i2c_transfer(chip->client->adapter, msg, 1);

	/* i2c_transfer returns number of messages transferred */
	if (ret != 1) {
		nu_err("write_block failed[%x]\n", reg);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int nu2115_read_block(struct nu2115_device *chip,
	u8 *value, u8 reg, unsigned int num_bytes)
{
	struct i2c_msg msg[MSG_LEN];
	u8 buf = 0;
	int ret = 0;

	if (chip == NULL || value == NULL) {
		nu_err("chip is null or value is null\n");
		return -EIO;
	}

	if (chip->chip_already_init == 0) {
		nu_err("chip not init\n");
		return -EIO;
	}

	buf = reg;

	msg[0].addr = chip->client->addr;
	msg[0].flags = 0;
	msg[0].buf = &buf;
	msg[0].len = 1;

	msg[1].addr = chip->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = value;
	msg[1].len = num_bytes;

	ret = i2c_transfer(chip->client->adapter, msg, MSG_LEN);

	/* i2c_transfer returns number of messages transferred */
	if (ret != MSG_LEN) {
		nu_err("read_block failed[%x]\n", reg);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	} else {
		return 0;
	}
}

static int nu2115_write_byte(struct nu2115_device *chip, u8 reg, u8 value)
{
	 /* 2 bytes offset 1 contains the data offset 0 is used by i2c_write */
	u8 temp_buffer[MSG_LEN] = {0};

	/* offset 1 contains the data */
	temp_buffer[1] = value;
	return nu2115_write_block(chip, temp_buffer, reg, 1);
}

static int nu2115_read_byte(struct nu2115_device *chip, u8 reg, u8 *value)
{
	return nu2115_read_block(chip, value, reg, 1);
}

static int nu2115_write_mask(struct nu2115_device *chip, u8 reg, u8 mask, u8 shift, u8 value)
{
	int ret = 0;
	u8 val = 0;

	ret = nu2115_read_byte(chip, reg, &val);
	if (ret < 0)
		return ret;

	val &= ~mask;
	val |= ((value << shift) & mask);

	ret = nu2115_write_byte(chip, reg, val);

	return ret;
}

static inline void nu2115_print_mem(struct nu2115_device *chip, char *buffer, unsigned int len)
{
	unsigned int i = 0;
	char buf[256] = {0,};

	memset(buf, 0, sizeof(buf));
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "NU2115-0x00: ");

	for (i = 0; i < len; i++) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "0x%02X ", buffer[i]);

		if ((i != 0) && ((i + 1) % 8 == 0)) {
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "	");
		}

		if ((i != 0) && ((i + 1) % 16 == 0)) {
			nu_info("%s\n", buf);
			memset(buf, 0, sizeof(buf));
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "NU2115-0x%02X: ", (i + 1));
		}
	}

	nu_info("%s\n", buf);

}

__maybe_unused static void nu2115_dump_register(struct nu2115_device *chip, char *out_reg, int len)
{
	u8 i = 0;
	int ret = 0;
	char val[NU2115_DEGLITCH_REG + 1] = {0, };

	for (i = 0; i < NU2115_DEGLITCH_REG; ++i) {
		ret = nu2115_read_byte(chip, i, val + i);
		if (ret)
			nu_err("dump_register read fail\n");
	}

	if (out_reg)
		memcpy(out_reg, val, (sizeof(val) > len) ? len : sizeof(val));

	nu2115_print_mem(chip, val, sizeof(val));
}

static int nu2115_reg_reset(struct nu2115_device *chip)
{
	int ret;
	u8 reg = 0;

	ret = nu2115_write_mask(chip, NU2115_CONTROL_REG,
		NU2115_REG_RST_MASK, NU2115_REG_RST_SHIFT,
		NU2115_REG_RST_ENABLE);
	if (ret)
		return -1;

	ret = nu2115_read_byte(chip, NU2115_CONTROL_REG, &reg);
	if (ret)
		return -1;

	nu_info("reg_reset [%x]=0x%x\n", NU2115_CONTROL_REG, reg);
	return 0;
}


static int nu2115_set_enable_chg(void * arg, unsigned int en)
{
	int ret = 0, en_now = 0;
	u8 reg = 0;
	u8 value = en ? 0x1 : 0x0;
	struct nu2115_device *chip = (struct nu2115_device *)arg;

	chip->chip_is_enable = en;

	/*check is already enable?*/
	ret = nu2115_read_byte(chip, NU2115_CHRG_CTL_REG, &reg);
	if (ret) {
		nu_err("nu2115_get_enable_chg error\n");
		return ret;
	}

	en_now = (reg & NU2115_CHARGE_EN_MASK) >> NU2115_CHARGE_EN_SHIFT;

	if (!!en_now == !!en) {
		chip->chip_enabled = !!en;
		nu_info("charger is already %d, reg[0x%02X]=0x%02X\n", en, NU2115_CHRG_CTL_REG, reg);
		return 0;
	}

	ret = nu2115_write_mask(chip, NU2115_CHRG_CTL_REG,
		NU2115_CHARGE_EN_MASK, NU2115_CHARGE_EN_SHIFT,
		value);
	if (ret) {
		nu_err("set CHARGE_EN failed ret=%d\n", ret);
	}

	ret = nu2115_read_byte(chip, NU2115_CHRG_CTL_REG, &reg);
	if (ret) {
		nu_err("read CHARGE_EN failed ret=%d\n", ret);
	}

	nu_info("charge_enable [%x]=0x%x\n", NU2115_CHRG_CTL_REG, reg);

	if (en) {
		ret = nu2115_write_byte(chip, NU2115_ADC_CTRL_REG,
			NU2115_ADC_CTRL_REG_INIT);

		if (chip->hw_mode == NU2115_SLAVE)
			ret = nu2115_write_byte(chip, NU2115_ADC_FN_DIS_REG,
				NU2115_ADC_FN_DIS_REG_INIT_SLAVE);
		else
			ret = nu2115_write_byte(chip, NU2115_ADC_FN_DIS_REG,
				NU2115_ADC_FN_DIS_REG_INIT_MASTER);

	} else {
		ret = nu2115_write_byte(chip, NU2115_ADC_CTRL_REG,
			NU2115_ADC_CTRL_REG_EXIT);
	}

	chip->chip_enabled = en_now;

	return 0;
}

static int nu2115_get_enable_chg(void * arg, unsigned int *en)
{
	int ret;
	u8 reg;
	struct nu2115_device *chip = (struct nu2115_device *)arg;

	ret = nu2115_read_byte(chip, NU2115_CHRG_CTL_REG, &reg);
	if (ret) {
		nu_err("nu2115_get_enable_chg error\n");
		return -1;
	}
	*en = (reg & NU2115_CHARGE_EN_MASK) >> NU2115_CHARGE_EN_SHIFT;

	dev_info(chip->dev, "%s %d, ret(%d)\n", __func__, *en, ret);
	return ret;
}

/*static int nu2115_discharge(struct nu2115_device *chip, int enable)
{
	int ret;
	u8 reg = 0;
	u8 value = enable ? 0x1 : 0x0;

	ret = nu2115_write_mask(chip, NU2115_BUS_OVP_REG,
		NU2115_VBUS_PD_EN_MASK, NU2115_VBUS_PD_EN_SHIFT,
		value);
	if (ret)
		return -1;

	ret = nu2115_read_byte(chip, NU2115_CONTROL_REG, &reg);
	if (ret)
		return -1;

	nu_info("discharge [%x]=0x%x\n", NU2115_CONTROL_REG, reg);
	return 0;
}*/

/*static int nu2115_is_device_close(struct nu2115_device *chip)
{
	u8 reg = 0;
	int ret = 0;

	ret = nu2115_read_byte(chip, NU2115_CHRG_CTL_REG, &reg);
	if (ret)
		return 1;

	if (reg & NU2115_CHARGE_EN_MASK)
		return 0;

	return 1;
}*/

static int nu2115_get_device_id(struct nu2115_device *chip)
{
	u8 part_info = 0;
	int ret = 0;

	if (chip == NULL) {
		nu_err("chip is null\n");
		return -1;
	}

	ret = nu2115_read_byte(chip, NU2115_PART_INFO_REG, &part_info);
	if (ret) {
		nu_err("get_device_id read fail\n");
		return -1;
	}

	nu_info("get_device_id [%x]=0x%x\n", NU2115_PART_INFO_REG, part_info);

	part_info = part_info & NU2115_DEVICE_ID_MASK;
	switch (part_info) {
	case NU2115_DEVICE_ID_NU2115:
		chip->device_id = 1;
		break;
	case NU2115_DEVICE_ID_SY6537C:
		chip->device_id = 2;
		break;
	default:
		chip->device_id = -1;
		nu_err("switchcap get dev_id fail\n");
		break;
	}

	return chip->device_id;
}

static int nu2115_get_vbat_mv(void *arg, unsigned int *vbat)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 reg_high = 0;
	u8 reg_low = 0;
	s16 voltage = 0;
	int ret = 0;

	if (!chip->chip_enabled) {
		*vbat = 0;
		return 0;
	}

	ret = nu2115_read_byte(chip, NU2115_VBAT_ADC1_REG, &reg_high);
	ret |= nu2115_read_byte(chip, NU2115_VBAT_ADC0_REG, &reg_low);
	if (ret)
		return -1;

	voltage = (reg_high << NU2115_LENTH_OF_BYTE) + reg_low;
	*vbat = (int)(voltage);

	if (chip->device_id == 2)
		*vbat = *vbat / 2;
	
	nu_info("VBAT_ADC1=0x%x, VBAT_ADC0=0x%x, vbat=%d\n", reg_high, reg_low, *vbat);

	return 0;
}

static int nu2115_get_ibat_ma(void *arg, unsigned int *ibat)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0;
	u8 reg_high = 0;
	u8 reg_low = 0;
	s16 curr = 0;

	if (!chip->chip_enabled) {
		*ibat = 0;
		return 0;
	}

	*ibat = 0;
	return 0;

	ret = nu2115_read_byte(chip, NU2115_IBAT_ADC1_REG, &reg_high);
	ret |= nu2115_read_byte(chip, NU2115_IBAT_ADC0_REG, &reg_low);
	if (ret)
		return -1;

	curr = (reg_high << NU2115_LENTH_OF_BYTE) + reg_low;
	*ibat = (int)(curr);

	if (chip->device_id == 2)
		*ibat = *ibat * 5 / 8;

	nu_info("IBAT_ADC1=0x%x, IBAT_ADC0=0x%x ibat=%d\n", reg_high, reg_low, *ibat);

	return 0;
}

static int nu2115_get_ibus_ma(void *arg, unsigned int *ibus)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 reg_high = 0;
	u8 reg_low = 0;
	int ret;
	int curr;

	if (!chip->chip_enabled) {
		*ibus = 0;
		return 0;
	}

	ret = nu2115_read_byte(chip, NU2115_IBUS_ADC1_REG, &reg_high);
	ret |= nu2115_read_byte(chip, NU2115_IBUS_ADC0_REG, &reg_low);
	if (ret)
		return -1;

	curr = (reg_high << NU2115_LENTH_OF_BYTE) + reg_low;
	*ibus = (int)(curr);

	if (chip->device_id == 2)
		*ibus = *ibus * 5 / 16;

	nu_info("IBUS_ADC1=0x%x, IBUS_ADC0=0x%x, ibus=%d\n", reg_high, reg_low, *ibus);

	return 0;
}

static int nu2115_get_vbus_mv(void *arg, unsigned int *vbus)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0;
	u8 reg_high = 0;
	u8 reg_low = 0;
	s16 voltage = 0;

	if (!chip->chip_enabled) {
		*vbus = 0;
		return 0;
	}

	ret = nu2115_read_byte(chip,NU2115_VBUS_ADC1_REG, &reg_high);
	ret |= nu2115_read_byte(chip,NU2115_VBUS_ADC0_REG, &reg_low);
	if (ret)
		return -1;

	voltage = (reg_high << NU2115_LENTH_OF_BYTE) + reg_low;
	*vbus = (int)(voltage);

	nu_info("VBUS_ADC1=0x%x, VBUS_ADC0=0x%x, vbus=%d\n", reg_high, reg_low, *vbus);

	return 0;
}

static int nu2115_config_watchdog(struct nu2115_device *chip, int enable)
{
	u8 reg;
	int ret;
	u8 value = enable ? 0x0 : 0x1;

	ret = nu2115_write_mask(chip, NU2115_CONTROL_REG,
		NU2115_WATCHDOG_DIS_MASK, NU2115_WATCHDOG_DIS_SHIFT,
		value);
	if (ret)
		return -1;

	ret = nu2115_read_byte(chip, NU2115_CONTROL_REG, &reg);
	if (ret)
		return -1;

	nu_info("config_watchdog [%x]=0x%x\n",
		NU2115_CONTROL_REG, reg);

	return 0;
}

static int nu2115_set_ibat_sns_res(void * arg, unsigned int isns_res_mo)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0, sns_res = 0;

		sns_res =  (isns_res_mo == 2) ?
			NU2115_IBAT_SNS_RESISTOR_2MOHM	: NU2115_IBAT_SNS_RESISTOR_5MOHM;
		nu_err("set bq ibat_sns_res %d(1:5MOHM,0:2MOHM)\n", sns_res);
		ret = nu2115_write_mask(chip, NU2115_DEGLITCH_REG,
			NU2115_IBAT_SNS_RES_MASK, NU2115_IBAT_SNS_RES_SHIFT,
			sns_res);
		if (ret) {
			nu_err("set_ibat_sns_res error ret=%d\n", ret);
			return -1;
		}

	return 0;
}

static int nu2115_set_vbatovp(void * arg, unsigned int mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mV < NU2115_BAT_OVP_BASE_3500MV)
		mV = NU2115_BAT_OVP_BASE_3500MV;

	if (mV > NU2115_BAT_OVP_MAX_5075MV)
		mV = NU2115_BAT_OVP_MAX_5075MV;

	value = (u8)((mV - NU2115_BAT_OVP_BASE_3500MV) * 10 /
		NU2115_BAT_OVP_STEP);
	ret = nu2115_write_mask(chip, NU2115_BAT_OVP_REG,
		NU2115_BAT_OVP_MASK, NU2115_BAT_OVP_SHIFT,
		value);
	if (ret) {
		nu_err("config_vbat_ovp_threshold_mv error ret=%d\n", ret);
		return -1;
	}

	nu_info("config_vbat_ovp_threshold_mv [0x%02X]=0x%02X, set %d mV\n",
		NU2115_BAT_OVP_REG, value, mV);

	return 0;

}

static int nu2115_get_vbatovp(void * arg, unsigned int *mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;

	ret = nu2115_read_byte(chip, NU2115_BAT_OVP_REG, &value);
	if (ret) {
		nu_err("get_vbatovp error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BAT_OVP_MASK) >> NU2115_BAT_OVP_SHIFT;

	*mV = value * NU2115_BAT_OVP_STEP / 10 + NU2115_BAT_OVP_BASE_3500MV;

	nu_info("reg(0x%02X)=0x%02X, vbatovp=%d(mV)\n", NU2115_BAT_OVP_REG, value, *mV);

	return 0;
}

static int nu2115_set_vbatovp_alarm(void * arg, unsigned int mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	
	u8 value;
	int ret = 0;

	if (mV < NU2115_BAT_OVP_ALM_BASE)
		mV = NU2115_BAT_OVP_ALM_BASE;

	if (mV > NU2115_BAT_OVP_ALM_MAX)
		mV = NU2115_BAT_OVP_ALM_MAX;

	value = (u8)((mV - NU2115_BAT_OVP_ALM_BASE) /
		NU2115_BAT_OVP_ALM_STEP);
	ret = nu2115_write_mask(chip, NU2115_BAT_OVP_ALM_REG,
		NU2115_BAT_OVP_ALM_MASK, NU2115_BAT_OVP_ALM_SHIFT,
		value);
	if (ret) {
		nu_err("nu2115_set_vbatovp_alarm error ret=%d\n", ret);
		return -1;
	}

	nu_info("set_vbatovp_alarm_mv [0x%02X]=0x%02X, set %d mV\n",
		NU2115_BAT_OVP_ALM_REG, value, mV);

	return 0;
}

static int nu2115_get_vbatovp_alarm(void * arg, unsigned int *mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;
	
	ret = nu2115_read_byte(chip, NU2115_BAT_OVP_ALM_REG, &value);
	if (ret) {
		nu_err("nu2115_get_vbatovp_alarm error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BAT_OVP_ALM_MASK) >> NU2115_BAT_OVP_ALM_SHIFT;

	*mV = value * NU2115_BAT_OVP_ALM_STEP + NU2115_BAT_OVP_ALM_BASE;

	nu_info("reg(0x%02X)=0x%02X, vbatovp_alarm=%d(mV)\n", NU2115_BAT_OVP_ALM_REG, value, *mV);

	return 0;
}

static int nu2115_set_ibatocp(void * arg, unsigned int mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mA < NU2115_BAT_OCP_BASE_2000MA)
		mA = NU2115_BAT_OCP_BASE_2000MA;

	if (mA > NU2115_BAT_OCP_MAX_14700MA)
		mA = NU2115_BAT_OCP_MAX_14700MA;

	value = (u8)((mA - NU2115_BAT_OCP_BASE_2000MA) /
		NU2115_BAT_OCP_STEP);
	ret = nu2115_write_mask(chip, NU2115_BAT_OCP_REG,
		NU2115_BAT_OCP_MASK, NU2115_BAT_OCP_SHIFT,
		value);
	if (ret) {
		nu_err("config_ibat_ocp_threshold_ma error ret=%d", ret);
		return -1;
	}

	nu_info("config_ibat_ocp_threshold_ma [0x%02X]=0x%02X, set %d mA\n",
		NU2115_BAT_OCP_REG, value, mA);

	return 0;
}

static int nu2115_get_ibatocp(void * arg, unsigned int *mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;

	ret = nu2115_read_byte(chip, NU2115_BAT_OCP_REG, &value);
	if (ret) {
		nu_err("get_ibatocp error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BAT_OCP_MASK) >> NU2115_BAT_OCP_SHIFT;

	*mA = value * NU2115_BAT_OCP_STEP + NU2115_BAT_OCP_BASE_2000MA;

	nu_info("reg(0x%02X)=0x%02X, ibatocp=%d(mA)\n", NU2115_BAT_OCP_REG, value, *mA);

	return 0;
}


static int nu2115_set_ibatocp_alarm(void * arg, unsigned int mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mA < NU2115_BAT_OCP_ALM_BASE)
		mA = NU2115_BAT_OCP_ALM_BASE;

	if (mA > NU2115_BAT_OCP_ALM_MAX)
		mA = NU2115_BAT_OCP_ALM_MAX;

	value = (u8)((mA - NU2115_BAT_OCP_ALM_BASE) /
		NU2115_BAT_OCP_ALM_STEP);
	ret = nu2115_write_mask(chip, NU2115_BAT_OCP_ALM_REG,
		NU2115_BAT_OCP_ALM_MASK, NU2115_BAT_OCP_ALM_SHIFT,
		value);
	if (ret) {
		nu_err("set_ibatocp_alarm error ret=%d\n", ret);
		return -1;
	}

	nu_info("set_ibatocp_alarm [0x%02X]=0x%02X, set %d mA\n",
		NU2115_BAT_OCP_ALM_REG, value, mA);

	return 0;

}

static int nu2115_get_ibatocp_alarm(void * arg, unsigned int *mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;
	
	ret = nu2115_read_byte(chip, NU2115_BAT_OCP_ALM_REG, &value);
	if (ret) {
		nu_err("get_ibatocp_alarm error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BAT_OCP_ALM_MASK) >> NU2115_BAT_OCP_ALM_SHIFT;

	*mA = value * NU2115_BAT_OCP_ALM_STEP + NU2115_BAT_OCP_ALM_BASE;

	nu_info("reg(0x%02X)=0x%02X, ibatocp_alarm=%d(mA)\n", NU2115_BAT_OCP_ALM_REG, value, *mA);

	return 0;
}

static int nu2115_set_vacovp(void * arg, unsigned int mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mV < NU2115_AC_OVP_BASE_10000MV)
		mV = NU2115_AC_OVP_BASE_10000MV;

	if (mV > NU2115_AC_OVP_MAX_13000MV)
		mV = NU2115_AC_OVP_MAX_13000MV;

	value = (u8)((mV - NU2115_AC_OVP_BASE_10000MV) /
		NU2115_AC_OVP_STEP);
	ret = nu2115_write_mask(chip, NU2115_AC_OVP_REG,
		NU2115_AC_OVP_MASK, NU2115_AC_OVP_SHIFT,
		value);
	if (ret) {
		nu_err("config_ac_ovp_threshold_mv error ret=%d\n", ret);
		return -1;
	}

	nu_info("config_ac_ovp_threshold_mv [0x%02X]=0x%02X, set %d mV\n",
		NU2115_AC_OVP_REG, value, mV);

	return 0;

}

static int nu2115_get_vacovp(void * arg, unsigned int *mV)
{
	u8 value = 0;
	int ret = 0;
	struct nu2115_device *chip = (struct nu2115_device *)arg;

	ret = nu2115_read_byte(chip, NU2115_AC_OVP_REG, &value);
	if (ret) {
		nu_err("get_vacovp error ret=%d\n", ret);
		return -1;
	}
	
	value = (value & NU2115_AC_OVP_MASK) >> NU2115_AC_OVP_SHIFT;

	*mV = value * NU2115_AC_OVP_STEP + NU2115_AC_OVP_BASE_10000MV;

	nu_info("reg(0x%02X)=0x%02X, vacovp=%d(mV)\n", NU2115_AC_OVP_REG, value, *mV);

	return 0;
}

static int nu2115_set_vbusovp(void * arg, u32 mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mV < NU2115_BUS_OVP_BASE_6000MV)
		mV = NU2115_BUS_OVP_BASE_6000MV;

	if (mV > NU2115_BUS_OVP_MAX_12300MV)
		mV = NU2115_BUS_OVP_MAX_12300MV;

	value = (u8)((mV - NU2115_BUS_OVP_BASE_6000MV) /
		NU2115_BUS_OVP_STEP);
	ret = nu2115_write_mask(chip, NU2115_BUS_OVP_REG,
		NU2115_BUS_OVP_MASK, NU2115_BUS_OVP_SHIFT,
		value);
	if (ret) {
		nu_err("config_vbus_ovp_threshold_mv error ret=%d\n", ret);
		return -1;
	}

	nu_info("config_vbus_ovp_threshold_mv [0x%02X]=0x%02X, set %d mV\n",
		NU2115_BUS_OVP_REG, value, mV);

	return 0;

}

static int nu2115_get_vbusovp(void * arg, u32 *mV)
{
	u8 value = 0;
	int ret = 0;
	struct nu2115_device *chip = (struct nu2115_device *)arg;

	ret = nu2115_read_byte(chip, NU2115_BUS_OVP_REG, &value);
	if (ret) {
		nu_err("get_vbusovp error ret=%d\n", ret);
		return -1;
	}
	
	value = (value & NU2115_BUS_OVP_MASK) >> NU2115_BUS_OVP_SHIFT;

	*mV = value * NU2115_BUS_OVP_STEP + NU2115_BUS_OVP_BASE_6000MV;

	nu_info("reg(0x%02X)=0x%02X, vbusovp=%d(mV)\n", NU2115_BUS_OVP_REG, value, *mV);

	return 0;
}

static int nu2115_set_vbusovp_alarm(void * arg, u32 mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	
	u8 value;
	int ret = 0;

	if (mV < NU2115_BUS_OVP_ALM_BASE)
		mV = NU2115_BUS_OVP_ALM_BASE;

	if (mV > NU2115_BUS_OVP_ALM_MAX)
		mV = NU2115_BUS_OVP_ALM_MAX;

	value = (u8)((mV - NU2115_BUS_OVP_ALM_BASE) /
		NU2115_BUS_OVP_ALM_STEP);
	ret = nu2115_write_mask(chip, NU2115_BUS_OVP_ALM_REG,
		NU2115_BUS_OVP_ALM_MASK, NU2115_BUS_OVP_ALM_SHIFT,
		value);
	if (ret) {
		nu_err("set_vbusovp_alarm error ret=%d\n", ret);
		return -1;
	}

	nu_info("set_vbusovp_alarm [0x%02X]=0x%02X, set %d mV\n",
		NU2115_BUS_OVP_ALM_REG, value, mV);

	return 0;
}

static int nu2115_get_vbusovp_alarm(void * arg, u32 *mV)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;
	
	ret = nu2115_read_byte(chip, NU2115_BUS_OVP_ALM_REG, &value);
	if (ret) {
		nu_err("get_vbusovp_alarm error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BUS_OVP_ALM_MASK) >> NU2115_BUS_OVP_ALM_SHIFT;

	*mV = value * NU2115_BUS_OVP_ALM_STEP + NU2115_BUS_OVP_ALM_BASE;

	nu_info("reg(0x%02X)=0x%02X, vbusovp_alarm=%d(mV)\n", NU2115_BUS_OVP_ALM_REG, value, *mV);

	return 0;
}

static int nu2115_set_ibusocp(void * arg, u32 mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value;
	int ret = 0;

	if (mA < NU2115_BUS_OCP_BASE_2500MA)
		mA = NU2115_BUS_OCP_BASE_2500MA;

	if (mA > NU2115_BUS_OCP_MAX_6250MA)
		mA = NU2115_BUS_OCP_MAX_6250MA;

	value = (u8)((mA - NU2115_BUS_OCP_BASE_2500MA) /
		NU2115_BUS_OCP_STEP);
	ret = nu2115_write_mask(chip, NU2115_BUS_OCP_UCP_REG,
		NU2115_BUS_OCP_MASK, NU2115_BUS_OCP_SHIFT,
		value);
	if (ret) {
		nu_info("config_ibus_ocp_threshold_ma error ret=%d\n", ret);
		return -1;
	}

	nu_info("config_ibus_ocp_threshold_ma [0x%02X]=0x%02X, set %d mA\n",
		NU2115_BUS_OCP_UCP_REG, value, mA);

	return 0;

}

static int nu2115_get_ibusocp(void * arg, u32 *mA)
{
	u8 value = 0;
	int ret = 0;
	struct nu2115_device *chip = (struct nu2115_device *)arg;

	ret = nu2115_read_byte(chip, NU2115_BUS_OCP_UCP_REG, &value);
	if (ret) {
		nu_err("get_ibusocp error ret=%d\n", ret);
		return -1;
	}
	
	value = (value & NU2115_BUS_OCP_MASK) >> NU2115_BUS_OCP_SHIFT;

	*mA = value * NU2115_BUS_OCP_STEP + NU2115_BUS_OCP_BASE_2500MA;

	nu_info("reg(0x%02X)=0x%02X, ibusocp=%d(mA)\n", NU2115_BUS_OCP_UCP_REG, value, *mA);

	return 0;
}

static int nu2115_set_ibusocp_alarm(void * arg, u32 mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	
	u8 value;
	int ret = 0;

	if (mA < NU2115_BUS_OCP_ALM_BASE)
		mA = NU2115_BUS_OCP_ALM_BASE;

	if (mA > NU2115_BUS_OCP_ALM_MAX)
		mA = NU2115_BUS_OCP_ALM_MAX;

	value = (u8)((mA - NU2115_BUS_OCP_ALM_BASE) /
		NU2115_BUS_OCP_ALM_STEP);
	ret = nu2115_write_mask(chip, NU2115_BUS_OCP_ALM_REG,
		NU2115_BUS_OCP_ALM_MASK, NU2115_BUS_OCP_ALM_SHIFT,
		value);
	if (ret) {
		nu_err("set_ibusocp_alarm error ret=%d\n", ret);
		return -1;
	}

	nu_info("set_ibusocp_alarm [0x%02X]=0x%02X, set %d mA\n",
		NU2115_BUS_OCP_ALM_REG, value, mA);

	return 0;
}

static int nu2115_get_ibusocp_alarm(void * arg, u32 *mA)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	u8 value = 0;
	int ret = 0;
	
	ret = nu2115_read_byte(chip, NU2115_BUS_OCP_ALM_REG, &value);
	if (ret) {
		nu_err("get_vbusovp_alarm error ret=%d\n", ret);
		return -1;
	}

	value = (value & NU2115_BUS_OCP_ALM_MASK) >> NU2115_BUS_OCP_ALM_SHIFT;

	*mA = value * NU2115_BUS_OCP_ALM_STEP + NU2115_BUS_OCP_ALM_BASE;

	nu_info("reg(0x%02X)=0x%02X, ibusocp_alarm=%d(mA)\n", NU2115_BUS_OCP_ALM_REG, value, *mA);

	return 0;
}

static int nu2115_get_interrupt_status(void * arg, unsigned int *status)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0;

	dev_info(chip->dev, "%s 0x%04X\n", __func__, *status);
	*status = chip->stat;
	return ret;
}

/*static int nu2115_chip_init(void)
{
	return 0;
}*/

static int nu2115_init_reg(void * arg)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0;
	u8 val = 0;

	ret = nu2115_write_byte(chip, NU2115_CONTROL_REG,
		NU2115_CONTROL_REG_INIT);
	ret |= nu2115_write_byte(chip, NU2115_CHRG_CTL_REG,
		NU2115_CHRG_CTL_REG_INIT);
	ret |= nu2115_write_byte(chip, NU2115_INT_MASK_REG,
		NU2115_INT_MASK_REG_INIT);
	ret |= nu2115_write_byte(chip, NU2115_FLT_MASK_REG,
		NU2115_FLT_MASK_REG_INIT);
	ret |= nu2115_write_byte(chip, NU2115_ADC_CTRL_REG,
		NU2115_ADC_CTRL_REG_EXIT);

	if (chip->hw_mode == NU2115_SLAVE) {
		ret |= nu2115_write_byte(chip, NU2115_ADC_FN_DIS_REG,
				NU2115_ADC_FN_DIS_REG_INIT_SLAVE);
		ret |= nu2115_write_mask(chip, NU2115_BAT_OVP_ALM_REG,
					NU2115_BAT_OVP_ALM_DIS_MASK, NU2115_BAT_OVP_ALM_DIS_SHIFT,
					NU2115_ALM_DISABLE);
		ret |= nu2115_write_mask(chip, NU2115_BAT_OCP_ALM_REG,
					NU2115_BAT_OCP_ALM_DIS_MASK, NU2115_BAT_OCP_ALM_DIS_SHIFT,
					NU2115_ALM_DISABLE);
		ret |= nu2115_write_mask(chip, NU2115_BAT_UCP_ALM_REG,
					NU2115_BAT_UCP_ALM_DIS_MASK, NU2115_BAT_UCP_ALM_DIS_SHIFT,
					NU2115_ALM_DISABLE);
	} else {
		ret |= nu2115_write_byte(chip, NU2115_ADC_FN_DIS_REG,
				NU2115_ADC_FN_DIS_REG_INIT_MASTER);
		ret |= nu2115_write_mask(chip, NU2115_BAT_OVP_ALM_REG,
					NU2115_BAT_OVP_ALM_DIS_MASK, NU2115_BAT_OVP_ALM_DIS_SHIFT,
					NU2115_ALM_ENABLE);
		ret |= nu2115_write_mask(chip, NU2115_BAT_OCP_ALM_REG,
					NU2115_BAT_OCP_ALM_DIS_MASK, NU2115_BAT_OCP_ALM_DIS_SHIFT,
					NU2115_ALM_ENABLE);
		ret |= nu2115_write_mask(chip, NU2115_BAT_UCP_ALM_REG,
					NU2115_BAT_UCP_ALM_DIS_MASK, NU2115_BAT_UCP_ALM_DIS_SHIFT,
					NU2115_ALM_ENABLE);
	}

	ret |= nu2115_write_mask(chip, NU2115_BUS_OVP_ALM_REG,
				NU2115_BUS_OVP_ALM_DIS_MASK, NU2115_BUS_OVP_ALM_DIS_SHIFT,
				NU2115_ALM_ENABLE);
	ret |= nu2115_write_mask(chip, NU2115_BUS_OCP_ALM_REG,
				NU2115_BUS_OCP_ALM_DIS_MASK, NU2115_BUS_OCP_ALM_DIS_SHIFT,
				NU2115_ALM_ENABLE);
	ret |= nu2115_set_ibat_sns_res(chip, 5);
	ret |= nu2115_set_ibatocp_alarm(chip, NU2115_IBAT_OCP_ALARM_THRESHOLD_INIT);
	ret |= nu2115_set_vbatovp(chip, NU2115_VBAT_OVP_THRESHOLD_INIT);
	ret |= nu2115_set_ibatocp(chip, NU2115_IBAT_OCP_THRESHOLD_INIT);
	ret |= nu2115_set_vacovp(chip, NU2115_AC_OVP_THRESHOLD_INIT);
	ret |= nu2115_set_vbusovp(chip, NU2115_VBUS_OVP_THRESHOLD_INIT);
	ret |= nu2115_set_ibusocp(chip, NU2115_IBUS_OCP_THRESHOLD_INIT);

	ret |= nu2115_write_mask(chip, NU2115_BAT_OCP_REG,
				NU2115_BAT_OCP_DIS_MASK, NU2115_BAT_OCP_DIS_SHIFT,
				NU2115_ALM_ENABLE);

	ret |= nu2115_write_mask(chip, NU2115_BAT_OCP_ALM_REG,
				NU2115_BAT_OCP_ALM_DIS_MASK, NU2115_BAT_OCP_ALM_DIS_SHIFT,
				NU2115_ALM_ENABLE);

	ret |= nu2115_write_mask(chip, NU2115_BAT_UCP_ALM_REG,
				NU2115_BAT_UCP_ALM_DIS_MASK, NU2115_BAT_UCP_ALM_DIS_SHIFT,
				NU2115_ALM_ENABLE);

	ret |= nu2115_write_byte(chip, 0x2F, 0x24);
	ret |= nu2115_write_byte(chip, 0x32, 0xF0);
	ret |= nu2115_write_byte(chip, 0x35, 0xC0);

	ret = nu2115_read_byte(chip, NU2115_PRESENT_DET_REG, &val);
	nu_info("init NU2115_PRESENT_DET_REG 0x%02X\n", val);

	if (ret) {
		nu_err("reg_init fail\n");
		return -1;
	}

	return 0;
}

static int nu2115_get_chg_type(void *arg, unsigned int *chg_type)
{
	arg = arg ? arg : NULL;

	*chg_type = SQC_PMIC_TYPE_CP21;

	return 0;
}

static int nu2115_get_chg_status(void *arg, unsigned int *abnormal_stat)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int ret = 0;
	u8 converter_state = 0;
	u8 ac_ovp = 0;
	u8 vbus_ovp_flag = 0;
	int chg_status = 0;

	ret = nu2115_read_byte(chip, NU2115_CONVERTER_STATE_REG, &converter_state);
	if (ret) {
		nu_err("get converter_state error ret=%d\n", ret);
		return -1;
	}

	ret = nu2115_read_byte(chip, NU2115_AC_OVP_REG, &ac_ovp);
	if (ret) {
		nu_err("get ac_ovp error ret=%d\n", ret);
		return -1;
	}

	ret = nu2115_read_byte(chip, NU2115_FLT_FLAG_REG, &vbus_ovp_flag);
	if (ret) {
		nu_err("get vbus_ovp_flag error ret=%d\n", ret);
		return -1;
	}
	nu_err("ac_ovp_state %d, ac_ovp_flag %d, vbus_ovp_flag %d\n",
			!!(ac_ovp & NU2115_AC_OVP_STAT_MASK),
			!!(ac_ovp & NU2115_AC_OVP_FLAG_MASK),
			!!(vbus_ovp_flag & NU2115_BUS_OVP_FLT_FLAG_MASK));

	if ((ac_ovp & NU2115_AC_OVP_STAT_MASK)
			|| (ac_ovp & NU2115_AC_OVP_FLAG_MASK)
			|| (vbus_ovp_flag & NU2115_BUS_OVP_FLT_FLAG_MASK)) {
		chg_status |= BIT(SQC_ERR_VBUS_OVP);
	} else {
		chg_status &= ~BIT(SQC_ERR_VBUS_OVP);
	}

	if (chip->chip_is_enable) {
		if (converter_state & NU2115_CONV_STAT_FLAG_MASK) {
			chg_status &= ~BIT(SQC_ERR_ENGINE_OTP);
		} else {
			chg_status |= BIT(SQC_ERR_ENGINE_OTP);
		}
	}

	/*0 is normal switching, 1 is not switching, deamon default is normal 0*/
	*abnormal_stat = chg_status;
	nu_err("switching abnormal_stat %d\n", *abnormal_stat);

	return 0;
}

static struct sqc_pmic_chg_ops nu2115_chg_ops = {

	.init_pmic_charger = nu2115_init_reg,

	.get_chg_type = nu2115_get_chg_type,
	.chg_enable = nu2115_set_enable_chg,
	.chg_enable_get = nu2115_get_enable_chg,

	.get_chg_status = nu2115_get_chg_status,
	.get_int_status = nu2115_get_interrupt_status,

	.chg_role_set = NULL,
	.chg_role_get = NULL,

	/*battery*/
	.batt_ovp_volt_set = nu2115_set_vbatovp,
	.batt_ovp_volt_get = nu2115_get_vbatovp,
	.batt_ovp_alm_volt_set = nu2115_set_vbatovp_alarm,
	.batt_ovp_alm_volt_get = nu2115_get_vbatovp_alarm,
	.batt_ocp_curr_set = nu2115_set_ibatocp,
	.batt_ocp_curr_get = nu2115_get_ibatocp,
	.batt_ocp_alm_curr_set = nu2115_set_ibatocp_alarm,
	.batt_ocp_alm_curr_get = nu2115_get_ibatocp_alarm,
	.batt_ibat_get = nu2115_get_ibat_ma,
	.batt_vbat_get = nu2115_get_vbat_mv,

	/*ac*/
	.ac_ovp_volt_set = nu2115_set_vacovp,
	.ac_ovp_volt_get = nu2115_get_vacovp,

	/*usb bus*/
	.usb_ovp_volt_set = nu2115_set_vbusovp,
	.usb_ovp_volt_get = nu2115_get_vbusovp,
	.usb_ovp_alm_volt_set = nu2115_set_vbusovp_alarm,
	.usb_ovp_alm_volt_get = nu2115_get_vbusovp_alarm,
	.usb_ocp_curr_set = nu2115_set_ibusocp,
	.usb_ocp_curr_get = nu2115_get_ibusocp,
	.usb_ocp_alm_curr_set = nu2115_set_ibusocp_alarm,
	.usb_ocp_alm_curr_get = nu2115_get_ibusocp_alarm,
	.usb_ibus_get = nu2115_get_ibus_ma,
	.usb_vbus_get = nu2115_get_vbus_mv,
};

static int nu2115_charge_init(struct nu2115_device *chip)
{
	if (chip == NULL) {
		nu_err("chip is null\n");
		return -1;
	}

	chip->device_id = nu2115_get_device_id(chip);
	if (chip->device_id == -1)
		return -1;

	nu_info("switchcap nu2115 device id is %d\n", chip->device_id);

	nu2115_init_finish_flag = NU2115_INIT_FINISH;
	return 0;
}

/*static int nu2115_charge_exit(struct nu2115_device *chip)
{
	int ret = 0;

	if (chip == NULL) {
		nu_err("chip is null\n");
		return -1;
	}

	ret = nu2115_set_enable_chg(chip, NU2115_SWITCHCAP_DISABLE);

	nu2115_init_finish_flag = NU2115_NOT_INIT;
	nu2115_int_notify_enable_flag = NU2115_DISABLE_INT_NOTIFY;

	usleep_range(10000, 11000);

	return ret;
}

static int nu2115_batinfo_exit(void)
{
	return 0;
}

static int nu2115_batinfo_init(void)
{
	int ret = 0;

	ret = nu2115_chip_init();
	if (ret) {
		pr_err("batinfo init fail\n");
		return -1;
	}

	return ret;
}*/
static void nu2115_check_status_flags(struct nu2115_device *chip)
{
    int ret;
	u8 sf_reg[22] = {0};

	nu_err("---------%s---------\n", __func__);

	ret = nu2115_read_block(chip, &sf_reg[0], NU2115_AC_OVP_REG, 2);
	if (ret == 0)
	{
	    if (sf_reg[0] & 0x80)
			nu_err("REG_05:%x, AC1_OVP_STAT\n", sf_reg[0]);

		if (sf_reg[0] & 0x40)
			nu_err("REG_05:%x, AC1_OVP_FLAG\n", sf_reg[0]);

		if (sf_reg[1] & 0x80)
			nu_err("REG_06:%x, AC2_OVP_STAT\n", sf_reg[1]);

		if (sf_reg[1] & 0x40)
			nu_err("REG_06:%x, AC2_OVP_FLAG\n", sf_reg[1]);
	}

	ret = nu2115_read_block(chip, &sf_reg[2], NU2115_BUS_OCP_UCP_REG, 11);

	if (ret == 0)
	{
	    if (sf_reg[2] & 0x40)
			nu_err("REG_09:%x, IBUS_UCP_RISE_FLAG\n", sf_reg[2]);

		if (sf_reg[3] & 0x40)
			nu_err("REG_0A:%x, IBUS_UCP_FALL_FLAG\n", sf_reg[3]);

		if (sf_reg[4] & 0x04)
			nu_err("REG_0B:%x, VOUT_OVP_STAT\n", sf_reg[4]);
		if (sf_reg[4] & 0x02)
			nu_err("REG_0B:%x, VOUT_OVP_FLAG\n", sf_reg[4]);
		

		if (sf_reg[5] & 0x80)
			nu_err("REG_0C:%x, TSD_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & 0x40)
			nu_err("REG_0C:%x, TSD_STAT\n", sf_reg[5]);
		if (sf_reg[5] & 0x20)
			nu_err("REG_0C:%x, VBUS_ERRLO_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & 0x10)
			nu_err("REG_0C:%x, VBUS_ERRHI_FLAG\n", sf_reg[5]);
        if (sf_reg[5] & 0x08)
			nu_err("REG_0C:%x, SS_TIMEOUT_FLAG\n", sf_reg[5]);
		if (sf_reg[5] & 0x04)
			nu_err("REG_0C:%x, CONV_ACTIVE_STAT\n", sf_reg[5]);
		if (sf_reg[5] & 0x01)
			nu_err("REG_0C:%x, PIN_DIAG_FAIL_FLAG\n", sf_reg[5]);

		if (sf_reg[6] & 0x08)
			nu_err("REG_0D:%x, WD_TIMEOUT_FLAG\n", sf_reg[6]);

		if (sf_reg[8] & 0x80)
			nu_err("REG_0F:%x, BAT_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x40)
			nu_err("REG_0F:%x, BAT_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x20)
			nu_err("REG_0F:%x, BUS_OVP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x10)
			nu_err("REG_0F:%x, BUS_OCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x08)
			nu_err("REG_0F:%x, BAT_UCP_ALM_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x02)
			nu_err("REG_0F:%x, VBAT_INSERT_STAT\n", sf_reg[8]);
		if (sf_reg[8] & 0x01)
			nu_err("REG_0F:%x, ADC_DONE_STAT\n", sf_reg[8]);

		if (sf_reg[9] & 0x80)
			nu_err("REG_10:%x, BAT_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x40)
			nu_err("REG_10:%x, BAT_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x20)
			nu_err("REG_10:%x, BUS_OVP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x10)
			nu_err("REG_10:%x, BUS_OCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x08)
			nu_err("REG_10:%x, BAT_UCP_ALM_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x02)
			nu_err("REG_10:%x, VBAT_INSERT_FLAG\n", sf_reg[9]);
		if (sf_reg[9] & 0x01)
			nu_err("REG_10:%x, ADC_DONE_FLAG\n", sf_reg[9]);

		if (sf_reg[11] & 0x80)
			nu_err("REG_12:%x, BAT_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x40)
			nu_err("REG_12:%x, BAT_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x20)
			nu_err("REG_12:%x, BUS_OVP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x10)
			nu_err("REG_12:%x, BUS_OCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x08)
			nu_err("REG_12:%x, BUS_RCP_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x04)
			nu_err("REG_12:%x, TS_ALM_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x02)
			nu_err("REG_12:%x, TS_FLT_STAT\n", sf_reg[11]);
		if (sf_reg[11] & 0x01)
			nu_err("REG_12:%x, TDIE_ALM_STAT\n", sf_reg[11]);

		if (sf_reg[12] & 0x80)
			nu_err("REG_13:%x, BAT_OVP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x40)
			nu_err("REG_13:%x, BAT_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x20)
		{
			nu_err("REG_13:%x, BUS_OVP_FLT_FLAG\n", sf_reg[12]);
			ret = nu2115_update_status_flag(chip, (sf_reg[12] & 0x20), SQC_ERR_VBUS_OVP);
	        if (ret) 
			{
		        nu_err("### SQC_ERR_VBUS_OVP\n");
	        }
		}
		if (sf_reg[12] & 0x10)
			nu_err("REG_13:%x, BUS_OCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x08)
			nu_err("REG_13:%x, BUS_RCP_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x04)
			nu_err("REG_13:%x, TS_ALM_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x02)
			nu_err("REG_13:%x, TS_FLT_FLAG\n", sf_reg[12]);
		if (sf_reg[12] & 0x01)
			nu_err("REG_13:%x, TDIE_ALM_FLAG\n", sf_reg[12]);
	}

	ret = nu2115_read_block(chip, &sf_reg[13], NU2115_DEGLITCH_REG, 10);
	if (ret == 0)
	{		
		if (sf_reg[14] & 0x80)
		    nu_err("REG_2F:%x, VAC1PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & 0x40)
		    nu_err("REG_2F:%x, VAC1PRESENT_FLAG\n", sf_reg[14]);
		if (sf_reg[14] & 0x10)
		    nu_err("REG_2F:%x, VAC2PRESENT_STAT\n", sf_reg[14]);
		if (sf_reg[14] & 0x08)
		    nu_err("REG_2F:%x, VAC2PRESENT_FLAG\n", sf_reg[14]);

		if (sf_reg[15] & 0x20)
		    nu_err("REG_30:%x, ACRB1_STAT\n", sf_reg[15]);
		if (sf_reg[15] & 0x10)
		    nu_err("REG_30:%x, ACRB1_FLAG\n", sf_reg[15]);
		if (sf_reg[15] & 0x04)
		    nu_err("REG_30:%x, ACRB2_STAT\n", sf_reg[15]);
		if (sf_reg[15] & 0x02)
		    nu_err("REG_30:%x, ACRB2_FLAG\n", sf_reg[15]);

        if (sf_reg[17] & 0x08)
		    nu_err("REG_32:%x, PMID2VOUT_UVP_FLAG\n", sf_reg[17]);
		
		if (sf_reg[17] & 0x04)
		    nu_err("REG_32:%x, PMID2VOUT_OVP_FLAG\n", sf_reg[17]);

		if (sf_reg[19] & 0x80)
		    nu_err("REG_34:%x, POWER_NG_FLAG\n", sf_reg[19]);

		if (sf_reg[21] & 0x80)
		    nu_err("REG_36:%x, VBUS_PRESENT_STAT\n", sf_reg[21]);
		if (sf_reg[21] & 0x40)
		    nu_err("REG_36:%x, VBUS_PRESENT_FLAG\n", sf_reg[21]);
	}

}

static void nu2115_interrupt_work(struct work_struct *work)
{
	struct nu2115_device *chip = NULL;

	chip = container_of(work, struct nu2115_device, irq_work);
	pm_stay_awake(chip->dev);

    nu2115_check_status_flags(chip);
	/* clear irq */
	enable_irq(chip->irq_int);
	pm_relax(chip->dev);
}

static irqreturn_t nu2115_interrupt(int irq, void *_chip)
{
	struct nu2115_device *chip = _chip;

	if (chip == NULL) {
		nu_err("chip is null\n");
		return -1;
	}

	if (chip->chip_already_init == 0)
		nu_err("chip not init\n");

	if (nu2115_init_finish_flag == NU2115_INIT_FINISH)
		nu2115_int_notify_enable_flag = NU2115_ENABLE_INT_NOTIFY;

	nu_info("irq triggered(%d)\n", nu2115_init_finish_flag);

	disable_irq_nosync(chip->irq_int);
	schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}

static int nu2115_get_hw_mode(struct nu2115_device *chip)
{
	int ret = 0, hw_mode = 0;
	u8 val = 0;

	ret = nu2115_read_byte(chip, NU2115_DEGLITCH_REG, &val);

	if (ret) {
		nu_err("Failed to read operation mode register\n");
		return ret;
	}

	val = (val & NU2115_MS_MASK) >> NU2115_MS_SHIFT;
	if (val == 0x00)
		hw_mode = NU2115_STDALONE;
	else if (val == 0x01)
		hw_mode = NU2115_SLAVE;
	else
		hw_mode = NU2115_MASTER;

	nu_info("work mode:%s\n", hw_mode == NU2115_STDALONE ? "Standalone" :
			(hw_mode == NU2115_SLAVE ? "Slave" : "Master"));

	return hw_mode;
}

static int nu2115_slave_chg_id_get(char *val, const void *arg)
{
	struct nu2115_device *chip = (struct nu2115_device *)arg;
	int slave_chg_id = 0;

	slave_chg_id = (cp1_init_finished && cp2_init_finished) ? 1 : 0;

	nu_info("cp1_init_finished: %d, cp1_init_finished: %d, slave_chg_id: %d\n",
				cp1_init_finished, cp2_init_finished, slave_chg_id);

	return snprintf(val, PAGE_SIZE, "%u", slave_chg_id);
}

static struct zte_misc_ops slave_chg_id_node = {
	.node_name = "slave_chg_id",
	.set = NULL,
	.get = nu2115_slave_chg_id_get,
	.free = NULL,
	.arg = NULL,
};

static int nu2115_chip_name_show(struct seq_file *m, void *v)
{
	struct nu2115_device *chip = m->private;

	if (chip) {
		seq_printf(m, "%s\n", chip->name);
	} else {
		seq_printf(m, "%s\n", "unkown");
	}
	return 0;
}

static int nu2115_chip_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, nu2115_chip_name_show, PDE_DATA(inode));
}

static const struct file_operations nu2115_chip_name_node = {
	.owner = THIS_MODULE,
	.open = nu2115_chip_name_open,
	.read = seq_read,
	.llseek = seq_lseek,
};

static int nu2115_otg_notifier(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct nu2115_device *chip =
		container_of(nb, struct nu2115_device, edv_nb);

	nu_info("extcon otg state: %d\n", extcon_get_state(chip->edev, EXTCON_USB_HOST));
	cancel_delayed_work(&chip->otg_work);
	schedule_delayed_work(&chip->otg_work, msecs_to_jiffies(0));

	return NOTIFY_DONE;
}

static void nu2115_otg_workfunc(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct nu2115_device *chip = container_of(dwork,
							  struct nu2115_device,
							  otg_work);
	int ret = -1;
	u8 val = 0;
	bool otg_present = false;

	if (!chip) {
		nu_err("chip is null failed\n");
		return;
	}
	otg_present = extcon_get_state(chip->edev, EXTCON_USB_HOST);
	nu_info("extcon otg_present: %d\n", otg_present);

	ret = nu2115_read_byte(chip, NU2115_PRESENT_DET_REG, &val);
	if (ret) {
		nu_err("failed read NU2115_PRESENT_DET_REG ret=%d\n", ret);
		return;
	}
	nu_info("NU2115_PRESENT_DET_REG +++++ 0x%02X\n", val);

	ret = nu2115_read_byte(chip, NU2115_ACDRV_CTRL_REG, &val);
	if (ret) {
		nu_err("failed read NU2115_ACDRV_CTRL_REG ret=%d\n", ret);
		return;
	}
	nu_info("NU2115_ACDRV_CTRL_REG +++++ 0x%02X\n", val);

	ret = nu2115_write_mask(chip, NU2115_PRESENT_DET_REG,
			NU2115_EN_OTG_MASK, NU2115_EN_OTG_SHIFT,
			otg_present);
	if (ret) {
		nu_err("failed write NU2115_PRESENT_DET_REG ret=%d\n", ret);
		return;
	}
	ret = nu2115_write_mask(chip, NU2115_ACDRV_CTRL_REG,
			NU2115_EN_ACDRV1_MASK, NU2115_EN_ACDRV1_SHIFT,
			otg_present);
	if (ret) {
		nu_err("failed write NU2115_ACDRV_CTRL_REG ret=%d\n", ret);
		return;
	}

	ret = nu2115_read_byte(chip, NU2115_PRESENT_DET_REG, &val);
	if (ret) {
		nu_err("failed read NU2115_PRESENT_DET_REG ret=%d\n", ret);
		return;
	}
	nu_info("NU2115_PRESENT_DET_REG ----- 0x%02X\n", val);

	ret = nu2115_read_byte(chip, NU2115_ACDRV_CTRL_REG, &val);
	if (ret) {
		nu_err("failed read NU2115_ACDRV_CTRL_REG ret=%d\n", ret);
		return;
	}
	nu_info("NU2115_ACDRV_CTRL_REG ----- 0x%02X\n", val);
}

static int nu2115_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct nu2115_device *chip = NULL;
	struct device_node *np = NULL;
	const struct of_device_id *match_table = NULL;
	struct sqc_pmic_chg_ops *sqc_ops = NULL;

	pr_info("nu2115 probe begin\n");

	if (client == NULL || id == NULL) {
		pr_err("nu2115 client or id is null\n");
		return -ENOMEM;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (chip == NULL) {
		pr_err("nu2115 devm_kzalloc failed\n");
		return -ENOMEM;
	}

	chip->edev = extcon_get_edev_by_phandle(&client->dev, 0);
	if (IS_ERR(chip->edev)) {
		pr_err("failed to find vbus extcon device.\n");
		return -EPROBE_DEFER;
	}

	match_table = of_match_node(nu2115_of_match, client->dev.of_node);
	if (match_table == NULL) {
		pr_err("nu2115 device tree match not found!\n");
		ret = -ENODEV;
		goto nu2115_fail_0;
	}

	chip->hw_mode = (long)match_table->data;

	nu_info("dtsmode=%ld, addr=0x%02X\n", chip->hw_mode, client->addr);

	chip->dev = &client->dev;
	np = chip->dev->of_node;
	chip->client = client;
	chip->name = "unkown";
	i2c_set_clientdata(client, chip);
	INIT_WORK(&chip->irq_work, nu2115_interrupt_work);

	if (chip->hw_mode == NU2115_STDALONE || chip->hw_mode == NU2115_MASTER) {
		INIT_DELAYED_WORK(&chip->otg_work, nu2115_otg_workfunc);
	}

	chip->gpio_int = of_get_named_gpio(np, "gpio_int", 0);
	nu_info("gpio_int=%d\n", chip->gpio_int);

	if (!gpio_is_valid(chip->gpio_int)) {
		nu_err("gpio(gpio_int) is not valid\n");
		ret = -EINVAL;
		goto nu2115_fail_0;
	}

	ret = gpio_request(chip->gpio_int, "nu2115_gpio_int");
	if (ret < 0) {
		nu_err("gpio(gpio_int) request fail\n");
		goto nu2115_fail_0;
	}

	ret = gpio_direction_input(chip->gpio_int);
	if (ret) {
		nu_err("gpio(gpio_int) set input fail\n");
		goto nu2115_fail_1;
	}

	chip->irq_int = gpio_to_irq(chip->gpio_int);
	if (chip->irq_int < 0) {
		nu_err("gpio(gpio_int) map to irq fail\n");
		ret = -EINVAL;
		goto nu2115_fail_1;
	}

	ret = request_irq(chip->irq_int, nu2115_interrupt,
		IRQF_TRIGGER_FALLING, "nu2115_int_irq", chip);
	if (ret) {
		nu_err("gpio(gpio_int) irq request fail\n");
		chip->irq_int = -1;
		goto nu2115_fail_1;
	}

	chip->chip_already_init = 1;

	if (nu2115_get_hw_mode(chip) != chip->hw_mode) {
		nu_err("nu2115 compare device hw mode failed!\n");
		ret = -ENODEV;
		goto nu2115_fail_2;
	}

	ret = nu2115_reg_reset(chip);
	if (ret) {
		nu_err("nu2115 reg reset fail\n");
		chip->chip_already_init = 0;
		goto nu2115_fail_2;
	}
	
	nu2115_charge_init(chip);

	ret = nu2115_init_reg(chip);
	if (ret) {
		nu_err("nu2115_init_reg fail\n");
		chip->chip_already_init = 0;
		goto nu2115_fail_2;
	}

	nu2115_config_watchdog(chip, 0);

	if (chip->hw_mode == NU2115_SLAVE) {
		sqc_ops = kzalloc(sizeof(struct sqc_pmic_chg_ops), GFP_KERNEL);
		memcpy(sqc_ops, &nu2115_chg_ops, sizeof(struct sqc_pmic_chg_ops));
		sqc_ops->arg = (void *)chip;
		ret = sqc_hal_charger_register(sqc_ops, SQC_CHARGER_PARALLEL2);
		if (ret < 0) {
			nu_err("%s register sqc hal fail(%d)\n", __func__, ret);
			goto nu2115_fail_2;
		}
	} else {
		sqc_ops = kzalloc(sizeof(struct sqc_pmic_chg_ops), GFP_KERNEL);
		memcpy(sqc_ops, &nu2115_chg_ops, sizeof(struct sqc_pmic_chg_ops));
		sqc_ops->arg = (void *)chip;
		ret = sqc_hal_charger_register(sqc_ops, SQC_CHARGER_PARALLEL1);
		if (ret < 0) {
			nu_err("%s register sqc hal fail(%d)\n", __func__, ret);
			goto nu2115_fail_2;
		}
	}
	if (chip->hw_mode == NU2115_STDALONE) {
		cp1_init_finished = 1;
		cp2_init_finished = 1;
		zte_misc_register_callback(&slave_chg_id_node, chip);
		chip->name = "nu2115";
		proc_create_data("driver/slave_chg_name", 0664, NULL,
			&nu2115_chip_name_node, chip);
	} else if (chip->hw_mode == NU2115_MASTER) {
		cp1_init_finished = 1;
		zte_misc_register_callback(&slave_chg_id_node, chip);
		chip->name = "nu2115";
		proc_create_data("driver/slave_chg_name", 0664, NULL,
			&nu2115_chip_name_node, chip);
	} else if (chip->hw_mode == NU2115_SLAVE) {
		cp2_init_finished = 1;
	} else {
		cp1_init_finished = 0;
		cp2_init_finished = 0;
	}

	if (chip->hw_mode == NU2115_STDALONE || chip->hw_mode == NU2115_MASTER) {
		chip->edv_nb.notifier_call = nu2115_otg_notifier;
		ret = extcon_register_notifier(chip->edev, EXTCON_USB_HOST,
							&chip->edv_nb);
		if (ret) {
			nu_err("failed to register extcon USB HOST notifier.\n");
			ret = -ENODEV;
			goto nu2115_fail_2;
		}
	}

	nu_info("%s hw_mode:%d probe end\n", __func__, chip->hw_mode);
	return 0;

nu2115_fail_2:
	free_irq(chip->irq_int, chip);
nu2115_fail_1:
	gpio_free(chip->gpio_int);
nu2115_fail_0:
	devm_kfree(&client->dev, chip);
	np = NULL;
	return ret;
}

static int nu2115_remove(struct i2c_client *client)
{
	struct nu2115_device *chip = i2c_get_clientdata(client);


	if (chip->irq_int)
		free_irq(chip->irq_int, chip);

	if (chip->gpio_int)
		gpio_free(chip->gpio_int);

	return 0;
}

static void nu2115_shutdown(struct i2c_client *client)
{
	struct nu2115_device *chip = i2c_get_clientdata(client);

	nu2115_reg_reset(chip);
}

/*MODULE_DEVICE_TABLE(i2c, nu2115);*/

static struct i2c_driver nu2115_driver = {
	.probe = nu2115_probe,
	.remove = nu2115_remove,
	.shutdown = nu2115_shutdown,
	.id_table = nu2115_i2c_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "nu2115",
		.of_match_table = of_match_ptr(nu2115_of_match),
	},
};

static int __init nu2115_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&nu2115_driver);
	if (ret)
		pr_err("i2c_add_driver error\n");

	return ret;
}

static void __exit nu2115_exit(void)
{
	i2c_del_driver(&nu2115_driver);
}

module_init(nu2115_init);
module_exit(nu2115_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("nu2115 module driver");
MODULE_AUTHOR("bing");
