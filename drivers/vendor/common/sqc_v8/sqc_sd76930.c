/*
 * Driver for the ETA Solutions SD76930 charger.
 * Author: Jinfeng.Lin1 <jinfeng.lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>

#include <sqc_common.h>
#include <vendor/common/zte_misc.h>
int sqc_notify_daemon_changed(int chg_id, int msg_type, int msg_val);

#define CHARGER_IC_TAG   "[ZTE_LDD_CHARGE][CHARGER_IC][SD76930]"

#define ZTE_CM_IC_INFO(fmt, args...) {pr_info(CHARGER_IC_TAG"[Info]%s %d: "fmt, __func__, __LINE__, ##args); }

#define ZTE_CM_IC_ERROR(fmt, args...) {pr_err(CHARGER_IC_TAG"[Error]%s %d:  "fmt, __func__, __LINE__, ##args); }

#define ZTE_CM_IC_DEBUG(fmt, args...) {pr_debug(CHARGER_IC_TAG"[Debug]%s %d:  "fmt, __func__, __LINE__, ##args); }




#define SD76930_REG_0					0x0
#define SD76930_REG_1					0x1
#define SD76930_REG_2					0x2
#define SD76930_REG_3					0x3
#define SD76930_REG_4					0x4
#define SD76930_REG_5					0x5
#define SD76930_REG_6					0x6
#define SD76930_REG_7					0x7
#define SD76930_REG_8					0x8
#define SD76930_REG_9					0x9
#define SD76930_REG_A					0xA
#define SD76930_REG_B					0xB
#define SD76930_REG_C					0xC
#define SD76930_REG_D					0xD
#define SD76930_REG_E					0xE
#define SD76930_REG_F					0xF
/* Register bits */
/* SD76930_REG_0 (0x00) */
#define SD76930_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define SD76930_REG_EN_HIZ_SHIFT		7

#define SD76930_REG_EN_OTG_MASK			GENMASK(6, 6)
#define SD76930_REG_EN_OTG_SHIFT		6
#define SD76930_REG_CHG_EN_MASK			GENMASK(5, 5)
#define SD76930_REG_CHG_EN_SHIFT			5

#define SD76930_REG_INPUT_CURRENT_MASK	GENMASK(4, 0)

#define SD76930_REG_INPUT_CURRENT_BASE	100
#define SD76930_REG_INPUT_CURRENT_LSB	100
#define SD76930_REG_INPUT_CURRENT_MIN	100
#define SD76930_REG_INPUT_CURRENT_MAX	3200

#define SD76930_EN_HIZ				(1)
#define SD76930_EXIT_HIZ			(0)
#define SD76930_EN_OTG 				(1)
#define SD76930_DISABLE_OTG			(0)

/* FAN5405_REG_1 (0x01)*/
#define SD76930_REG_WD_REST_MASK			GENMASK(7, 7)
#define SD76930_REG_WD_REST_SHIFT				(7)
#define SD76930_REG_OVP_MASK			GENMASK(6, 6)
#define SD76930_REG_OVP_SHIFT			6
#define SD76930_REG_IBAT_CURRENT_MASK	GENMASK(5, 0)

#define SD76930_REG_IBAT_CURRENT_BASE	0
#define SD76930_REG_IBAT_CURRENT_LSB	60
#define SD76930_REG_IBAT_CURRENT_MIN	0
#define SD76930_REG_IBAT_CURRENT_MAX	3780


/* SD76930_REG_2 (0x02)*/
#define SD76930_REG_VBAT_REG_MASK				GENMASK(7, 1)
#define SD76930_REG_VBAT_REG_SHIFT				(1)
#define SD76930_REG_VRCHG_MASK				GENMASK(0, 0)
#define SD76930_REG_VRCHG_SHIFT				0

#define SD76930_REG_VBAT_REG_LSB 				(8)
#define SD76930_REG_VBAT_REG_BASE 				(3856)
#define SD76930_REG_VBAT_REG_MIN 				(3856)
#define SD76930_REG_VBAT_REG_MAX 				(4752)


/* SD76930_REG_3 (0x03)*/
#define SD76930_REG_VENDOR_CODE_MASK			GENMASK(7, 5)
#define SD76930_REG_VENDOR_CODE_SHIFT			(5)
#define SD76930_REG_PN_CODE_MASK			GENMASK(4, 3)
#define SD76930_REG_PN_CODE_SHIFT			(3)
#define SD76930_REG_REV_CODE_MASK			GENMASK(2, 0)
#define SD76930_REG_REV_CODE_SHIFT			(0)

/* SD76930_REG_4 (0x04)*/
#define SD76930_REG_TERM_MASK				GENMASK(7, 7)
#define SD76930_REG_TERM_SHIFT				(7)
#define SD76930_REG_TIMER_MASK				GENMASK(6, 6)
#define SD76930_REG_TIMER_SHIFT				6
#define SD76930_REG_REST_MASK				GENMASK(5, 5)
#define SD76930_REG_REST_SHIFT				(5)
#define SD76930_REG_WDOG_EN_MASK			GENMASK(4, 4)
#define SD76930_REG_WDOG_EN_SHIFT			4
#define SD76930_REG_DPVOL_MASK				GENMASK(3, 2)
#define SD76930_REG_DMVOL_MASK				GENMASK(1, 0)


/* SD76930_REG_5 (0x05)*/
#define SD76930_REG_VBUS_GD_MASK			GENMASK(7, 7)
#define SD76930_REG_VDPM_MASK				GENMASK(6, 6)
#define SD76930_REG_IDPM_MASK				GENMASK(5, 5)
#define SD76930_REG_CHG_STAT_MASK		    GENMASK(4, 3)
#define SD76930_REG_CV_STAT_MASK			GENMASK(2, 2)
#define SD76930_REG_BOOT_STAT_MASK			GENMASK(1, 1)
#define SD76930_REG_THERM_STAT_MASK			GENMASK(0, 0)

/* SD76930_REG_6 (0x06) READ*/
#define SD76930_REG_WDOG_FLG_MASK				GENMASK(7, 7)
#define SD76930_REG_THERM_FLG_MASK				GENMASK(6, 6)
#define SD76930_REG_CHRG_FLG_MASK				GENMASK(5, 4)
#define SD76930_REG_BAT_OVP_FLG_MASK			GENMASK(3, 3)
#define SD76930_REG_BOST_FLG_MASK				GENMASK(1, 0)


/* SD76930_REG_7 (0x07)*/
#define SD76930_REG_THERMAL_MASK            GENMASK(7, 7)
#define SD76930_REG_VINDPM_MASK				GENMASK(6, 0)
#define SD76930_REG_VINDPM_SHIFT			 0

#define SD76930_REG_VINDPM_BASE				3900
#define SD76930_REG_VINDPM_LSB				100
#define SD76930_REG_VINDPM_MIN				3900
#define SD76930_REG_VINDPM_MAX				8200


/* SD76930_REG_8 (0x08)*/
#define SD76930_REG_CHG_TIME_MASK           GENMASK(6, 6)
#define SD76930_REG_BOOST_LIM_MASK			GENMASK(5, 4)
#define SD76930_REG_ITERM_MASK				GENMASK(3, 0)
#define SD76930_REG_ITERM_SHIFT				0

#define SD76930_REG_ITERM_LSB				60
#define SD76930_REG_ITERM_BASE				60
#define SD76930_REG_ITERM_MIN				60
#define SD76930_REG_ITERM_MAX				960

/* SD76930_REG_9 (0x09)*/
#define SD76930_REG_BUSOV_MASK              GENMASK(7, 7)
#define SD76930_REG_VBUS_P_MASK				GENMASK(6, 6)
#define SD76930_REG_BAT_COMP_MASK			GENMASK(5, 3)
#define SD76930_REG_COMP_MAX_MASK			GENMASK(2, 0)

/* SD76930_REG_A (0x0A)*/
#define SD76930_REG_DIS_OTGOCP_MASK         GENMASK(7, 7)
#define SD76930_REG_THM_REG_MASK			GENMASK(6, 5)
#define SD76930_REG_EN_THM_HYS_MASK			GENMASK(4, 4)
#define SD76930_REG_DIS_THM_MASK			GENMASK(3, 3)
#define SD76930_REG_THM_TIM_MASK			GENMASK(2, 1)

/* SD76930_REG_B (0x0B)*/
#define SD76930_REG_TOPOFF_ACTIVE_MASK      GENMASK(7, 7)
#define SD76930_REG_TOPOFF_TIMER_MASK		GENMASK(6, 5)

/* SD76930_REG_C (0x0C)*/
#define SD76930_REG_TR_HS_DRV_MASK         	GENMASK(5, 3)
#define SD76930_REG_TR_LS_DRV_MASK			GENMASK(2, 0)


/* SD76930_REG_D (0x0D)*/
#define SD76930_REG_TON_SETTING_MASK        GENMASK(2, 0)



#define SD76930_FGU_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define SD76930_OTG_VALID_MS				(500)
#define SD76930_FEED_WATCHDOG_VALID_MS			(50)
#define SD76930_WDG_TIMER_S				(15)

#define SD76930_OTG_TIMER_FAULT				(0x6)

#define SD76930_DISABLE_PIN_MASK_2730			BIT(0)
#define SD76930_DISABLE_PIN_MASK_2721			BIT(15)
#define SD76930_DISABLE_PIN_MASK_2720			BIT(0)

#define SD76930_CHG_IMMIN		(550000)
#define SD76930_CHG_IMSTEP		(200000)
#define SD76930_CHG_IMMAX		(3050000)
#define SD76930_CHG_IMIN		(550000)
#define SD76930_CHG_ISTEP		(100000)
#define SD76930_CHG_IMAX		(3050000)
#define SD76930_CHG_IMAX30500		(0x19)

#define SD76930_CHG_VMREG_MIN		(3856)
#define SD76930_CHG_VMREG_STEP		(8)
#define SD76930_CHG_VMREG_MAX		(4752)
#define SD76930_CHG_VOREG_MIN		(3500)
#define SD76930_CHG_VOREG_STEP		(20)
#define SD76930_CHG_VOREG_MAX		(4440)

#define SD76930_REG_VOREG		(BIT(5) | BIT(3))
#define SD76930_IIN_LIM_SEL		(1)
#define SD76930_REG_ICHG_OFFSET		(0)
#define SD76930_REG_IIN_LIMIT1_MAX	(3000000)

#define ENABLE_CHARGE 0
#define DISABLE_CHARGE 1


#define DEBUG_CHARGER_IC 1

#define SD76930_FCHG_OVP_6V5 6500
#define SD76930_FCHG_OVP_10V5 10500

#define SD76930_REG_NUM 16
#define SD76930_OTG_RETRY_TIMES			10

struct sd76930_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};

struct sd76930_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct sd76930_charge_current cur;
	struct mutex lock;
	bool charging;
	bool is_charger_online;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	bool otg_enable;
	struct alarm wdg_timer;
	bool need_reinit;
	int termination_cur;
	struct mutex i2c_rw_lock;
	struct usb_phy *usb_phy;
	int limit;
	bool ovp_enable;
	int vbus_ovp_mv;
	struct notifier_block usb_notify;
	bool enable_powerptah;
	int chg_status;
	bool is_charging_enabled;
	struct wakeup_source *sd_wake_lock;
	bool host_status_check;
	bool use_typec_extcon;
	int init_finished;
};


struct sd76930_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct sd76930_charger_reg_tab reg_tab[SD76930_REG_NUM + 1] = {
    {0, SD76930_REG_0, "Setting Input Limit Current reg"},
    {1, SD76930_REG_1, "Setting Charge Limit Current reg"},
    {2, SD76930_REG_2, "Setting Vbat Limit Voltage reg"},
    {3, SD76930_REG_3, "Vendor reg"},
    {4, SD76930_REG_4, "Setting term and WD timer"},
    {5, SD76930_REG_5, "STAT reg"},
    {6, SD76930_REG_6, "Fault reg"},
    {7, SD76930_REG_7, "VINDPM reg"},
    {8, SD76930_REG_8, "ITERM reg"},
    {9, SD76930_REG_9, "IR compensation reg"},
    {10, SD76930_REG_A, "Thermal Setting reg"},
    {11, SD76930_REG_B, "Topoff active and timer reg"},
    {12, SD76930_REG_C, "TR_HS TR_LS reg"},
    {13, SD76930_REG_D, "TON setting reg"},
    {14, SD76930_REG_E, "Chip ID reg"},
    {15, SD76930_REG_F, "Version reg"},
    {16, 0, "null"},
};

static int sd76930_charger_set_status(struct sd76930_charger_info *info, int val);

static int sd76930_charger_set_input_limit_current(struct sd76930_charger_info *info, u32 limit_cur);

static int sd76930_charger_feed_watchdog(struct sd76930_charger_info *info);

static int sd76930_read(struct sd76930_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sd76930_write(struct sd76930_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sd76930_update_bits(struct sd76930_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sd76930_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sd76930_write(info, reg, v);
}

static int sd76930_charger_set_ovp(struct sd76930_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol > 6500)
		reg_val = 0x1;
	else
		reg_val = 0x0;


	ZTE_CM_IC_INFO("vol:%d  reg_val: 0x%x\n", vol, reg_val);
	return sd76930_update_bits(info, SD76930_REG_1, SD76930_REG_OVP_MASK,
				   reg_val << SD76930_REG_OVP_SHIFT);
}

static int sd76930_charger_set_termina_vol(struct sd76930_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret;
	uint32_t data_temp = 0;

	if (vol < SD76930_CHG_VMREG_MIN)
		data_temp = SD76930_CHG_VMREG_MIN;
	else if (vol > SD76930_CHG_VMREG_MAX)
		data_temp = SD76930_CHG_VMREG_MAX;
	else
		data_temp = vol;
	reg_val = (data_temp - SD76930_CHG_VMREG_MIN) / SD76930_CHG_VMREG_STEP;

	ZTE_CM_IC_INFO("POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX vol:%d  reg_val: 0x%x\n",data_temp,reg_val << SD76930_REG_VBAT_REG_SHIFT);

	if(info->charging == true)
	{
		ZTE_CM_IC_INFO("set vbat max is charger\n");
		sd76930_charger_set_status(info,0);
		ret = sd76930_update_bits(info, SD76930_REG_2, SD76930_REG_VBAT_REG_MASK,(reg_val << SD76930_REG_VBAT_REG_SHIFT));
		sd76930_charger_set_status(info,1);
	} else {
		ZTE_CM_IC_INFO("set vbat max not charger\n");
		ret = sd76930_update_bits(info, SD76930_REG_2, SD76930_REG_VBAT_REG_MASK,(reg_val << SD76930_REG_VBAT_REG_SHIFT));
	}
		
	return ret;
}

static int sd76930_charger_hw_init(struct sd76930_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt, termination_cur;
	int ret;
	char chip_id_info;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		ZTE_CM_IC_ERROR("no battery information is supplied\n");

		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 1500000;
		info->cur.dcp_cur = 1500000;
		info->cur.cdp_limit = 1000000;
		info->cur.cdp_cur = 1000000;
		info->cur.unknown_limit = 500000;
		info->cur.unknown_cur = 500000;

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.44V.
		 */
		voltage_max_microvolt = 4400;
		termination_cur = 120;
		info->termination_cur = termination_cur;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	if (of_device_is_compatible(info->dev->of_node,
				    "bigmtech,sd76930")) {
/*read id*/
		ret = sd76930_read(info, SD76930_REG_F,&chip_id_info);
		if (ret) {
			ZTE_CM_IC_ERROR("sd76930 read chip id failed ret = %d\n", ret);
			return ret;
		}else if((chip_id_info&0xff)!=0xA1)
		{
			ZTE_CM_IC_ERROR(" chip id = 0x%02X\n", (chip_id_info&0xff));
			return -1;
		}
		ZTE_CM_IC_INFO("chip id = 0x%02X\n", (chip_id_info&0xff));
/*reset REG*/
		ret = sd76930_update_bits(info, SD76930_REG_4,
						SD76930_REG_REST_MASK,
						1 << SD76930_REG_REST_SHIFT);
		if (ret) {
			ZTE_CM_IC_ERROR("reset failed ret = %d\n", ret);
			return ret;
		}
/*WATCHDOG disable*/
		ret = sd76930_update_bits(info, SD76930_REG_4,
						SD76930_REG_WDOG_EN_MASK,
						0);
		if (ret) {
			ZTE_CM_IC_ERROR("reset failed ret = %d\n", ret);
			return ret;
		}
/*set vdmp 4.6V*/
		ret = sd76930_update_bits(info, SD76930_REG_7,
						SD76930_REG_VINDPM_MASK,
						7);
		if (ret) {
			ZTE_CM_IC_ERROR("reset failed ret = %d\n", ret);
			return ret;
		}
/*set Treg 110*/
		ret = sd76930_update_bits(info, SD76930_REG_7,
						SD76930_REG_THERMAL_MASK,
						SD76930_REG_THERMAL_MASK);
		if (ret) {
			ZTE_CM_IC_ERROR("set Thermal 110 failed ret = %d\n", ret);
			return ret;
		}
	}
	/*set max vbat*/
	ret = sd76930_charger_set_termina_vol(info, voltage_max_microvolt);
	if (ret) {
		ZTE_CM_IC_ERROR("set max vbat failed ret = %d\n", ret);
		return ret;
	}
	/*set ovp 10.5v*/
	ret = sd76930_charger_set_ovp(info, SD76930_FCHG_OVP_10V5);
	if (ret) {
		ZTE_CM_IC_ERROR("set  OVP 10v5 failed ret = %d\n", ret);
		return ret;
	}

	ret = sd76930_charger_set_input_limit_current(info, info->cur.unknown_cur);
	if (ret)
		ZTE_CM_IC_ERROR("set limit current failed ret = %d\n", ret);

	return ret;
}

static int sd76930_charger_start_charge(struct sd76930_charger_info *info)
{
	int ret = 0;

	ZTE_CM_IC_INFO("enter!\n");

	if (!IS_ERR(info->gpiod)) {
		gpiod_set_value_cansleep(info->gpiod, ENABLE_CHARGE);
	} else {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
			ZTE_CM_IC_ERROR("enable charge failed ret = %d\n", ret);
	}

	return ret;
}

static void sd76930_charger_stop_charge(struct sd76930_charger_info *info)
{
	int ret;

	ZTE_CM_IC_INFO("enter!\n");

	if (!IS_ERR(info->gpiod)) {
		ZTE_CM_IC_ERROR("gpiod failed!\n");
		gpiod_set_value_cansleep(info->gpiod, DISABLE_CHARGE);
	} else {
		ZTE_CM_IC_INFO("chg_pd enable\n");
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			ZTE_CM_IC_ERROR("disable charge failed ret = %d\n", ret);
	}
	
}

static int sd76930_charger_set_current(struct sd76930_charger_info *info, u32 cur)
{
	u8 reg_val ;

	ZTE_CM_IC_INFO(" cur= %d\n", cur);
	/*if ICHG_OFFSET = 1, chg cur + 100mA*/
	if(cur >= 3780000)
	{
		reg_val = 0x3f;
	} else {
		reg_val = cur / 60000;
	}

	ZTE_CM_IC_INFO("reg_val= 0x%x\n", reg_val);
	return sd76930_update_bits(info, SD76930_REG_1, SD76930_REG_IBAT_CURRENT_MASK,
				    reg_val);
}

static int sd76930_charger_get_current(struct sd76930_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sd76930_read(info, SD76930_REG_1, &reg_val);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("set ichg[2:0] cur failed ret = %d\n", ret);
		return ret;
	}
	reg_val = reg_val & 0x3f;
	if(reg_val >=0x3f) {
		*cur = 3780000;
	} else {
		*cur = 60000 * reg_val;
	}
	return 0;
}


/*
static int32_t sd76930_set_en_term_chg(struct sd76930_charger_info *info, int32_t enable)
{
	return sd76930_update_bits(info, SD76930_REG_4, SD76930_REG_TERM_MASK, !!enable << SD76930_REG_TERM_SHIFT);
}
*/


/*
static int32_t sd76930_set_en_chg_timer(struct sd76930_charger_info *chip,int32_t enable)
{
	return sd76930_update(chip, SD76930_REG_4, SD76930_REG_TIMER_MASK, !!enable << SD76930_REG_TIMER_SHIFT);
}
*/

/*
static int32_t sd76930_set_wd_reset(struct sd76930_charger_info *chip)
{
	ZTE_CM_IC_INFO("enter!\n");

	return sd76930_update(chip, SD76930_R01, 1, CON1_WD_MASK, CON1_WD_SHIFT);
}
*/
static int sd76930_charger_set_input_limit_current(struct sd76930_charger_info *info, u32 limit_cur)
{
	u8 reg_val;

	ZTE_CM_IC_INFO("limit_cur = %d\n", limit_cur);
	if(limit_cur >= 3200000)
	{
		reg_val = 0x1f;
	}else
	{
		reg_val = limit_cur / 100000 - 1;
	}
	ZTE_CM_IC_INFO("reg_val = 0x%x\n", reg_val);	
	return sd76930_update_bits(info, SD76930_REG_0, SD76930_REG_INPUT_CURRENT_MASK,
				    reg_val);
}

static int sd76930_charger_get_input_limit_current(struct sd76930_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = sd76930_read(info, SD76930_REG_0, &reg_val);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("failed ret = %d\n", ret);
		return ret;
	}
	ZTE_CM_IC_INFO("reg_val = 0x%x\n", reg_val);

	reg_val &= SD76930_REG_INPUT_CURRENT_MASK;
	*limit_cur = 100000 * (reg_val + 1 );

	ZTE_CM_IC_INFO("limit_cur = %d\n", *limit_cur);

	return ret;
}

static int sd76930_charger_get_limit_current(struct sd76930_charger_info *info, u32 *limit_cur)
{
	int ret;

	ret = sd76930_charger_get_input_limit_current(info, limit_cur);
	if (ret)
		ZTE_CM_IC_ERROR("failed ret = %d\n", ret);

	return ret;
}

static int sd76930_charger_get_health(struct sd76930_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sd76930_charger_get_status(struct sd76930_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sd76930_charger_set_status(struct sd76930_charger_info *info, int val)
{
	int ret = 0;

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sd76930_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sd76930_charger_start_charge(info);
		if (ret) {
			ZTE_CM_IC_ERROR("start charge failed\n");
		} else {
			info->charging = true;
		}
	}

	return ret;
}

static int sd76930_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sd76930_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, health;
	int ret = 0;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sd76930_charger_get_status(info);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sd76930_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sd76930_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sd76930_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sd76930_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sd76930_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		ZTE_CM_IC_ERROR(" NULL pointer!!!\n");
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sd76930_charger_set_current(info, val->intval );
		if (ret < 0)
			ZTE_CM_IC_ERROR("set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sd76930_charger_set_input_limit_current(info, val->intval);
		if (ret < 0)
			ZTE_CM_IC_ERROR("set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sd76930_charger_set_status(info, val->intval);
		if (ret < 0)
			ZTE_CM_IC_ERROR("set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sd76930_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			ZTE_CM_IC_ERROR("failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = sd76930_charger_set_status(info, val->intval);
		if (ret < 0)
			ZTE_CM_IC_ERROR("set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		ZTE_CM_IC_INFO("POWER_SUPPLY_PROP_PRESENT\n");
		info->is_charger_online = val->intval;
		if (val->intval == true)
		{
			ZTE_CM_IC_ERROR("POWER_SUPPLY_PROP_PRESENT start wdt\n");
			schedule_delayed_work(&info->wdt_work, 0);
		}else
		{
			cancel_delayed_work_sync(&info->wdt_work);
		}
			
		break;
	case POWER_SUPPLY_PROP_TYPE: //add
		ZTE_CM_IC_INFO("set ovp 6v5 CHANGE TO 10.5v\n");
		ret = sd76930_charger_set_ovp(info, SD76930_FCHG_OVP_10V5);
		
		if (ret)
			ZTE_CM_IC_ERROR("failed to set fast charge ovp\n");

		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sd76930_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_TYPE:

		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sd76930_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc sd76930_charger_desc = {
	.name			= "charger_psy",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sd76930_usb_props,
	.num_properties		= ARRAY_SIZE(sd76930_usb_props),
	.get_property		= sd76930_charger_usb_get_property,
	.set_property		= sd76930_charger_usb_set_property,
	.property_is_writeable	= sd76930_charger_property_is_writeable,
};

static void sd76930_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sd76930_charger_info *info = container_of(dwork,
							 struct sd76930_charger_info,
							 wdt_work);
	int ret;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return;
	}

	ret = sd76930_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 5);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static void sd76930_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sd76930_charger_info *info = container_of(dwork,
			struct sd76930_charger_info, otg_work);
	int ret;
	char reg_val;

	ret = sd76930_read(info, SD76930_REG_6,&reg_val);
	if (ret)
		ZTE_CM_IC_ERROR("sd76930 read SD76930_REG_6 failed ret = %d\n", ret);
	if((reg_val & SD76930_REG_BOST_FLG_MASK) == 0x11) {
		ZTE_CM_IC_INFO("OTG over current SD76930_REG_6 =0X%02X \n",reg_val);
		ret = sd76930_update_bits(info, SD76930_REG_0,SD76930_REG_EN_OTG_MASK,SD76930_REG_EN_OTG_MASK);
	} else {
		ZTE_CM_IC_INFO("SD76930_REG_6 =0X%02X \n",reg_val);
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int sd76930_charger_enable_otg(struct regulator_dev *dev)
{
	struct sd76930_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 reg_val;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_INFO("into\n");

	info->otg_enable = true;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
		if (ret) {
			ZTE_CM_IC_ERROR("failed to disable bc1.2 detect function. ret = %d\n", ret);
			return ret;
		}
		/*stop charger*/
		ret = sd76930_update_bits(info, SD76930_REG_0,
					   SD76930_REG_CHG_EN_MASK,
					   0);
		if (ret) {
			ZTE_CM_IC_ERROR("failed to disable charger. ret = %d\n", ret);
			return ret;
	}
	/*Set CHG_PD*/  
	if (!IS_ERR(info->gpiod)) {
		gpiod_set_value_cansleep(info->gpiod, ENABLE_CHARGE);
	} else {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
			ZTE_CM_IC_ERROR("Set CHG_PD VAL 0 failed ret = %d\n", ret);
	}
	/* disable EN_HIZ */
	ret = sd76930_update_bits(info, SD76930_REG_0,
				   SD76930_REG_EN_HIZ_MASK,
				   0);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to set hiz 0. ret = %d\n", ret);
		return ret;
	}
	/*enable otg*/
	ret = sd76930_update_bits(info, SD76930_REG_0,
				   SD76930_REG_EN_OTG_MASK,
				   SD76930_REG_EN_OTG_MASK);
	if (ret) {
		ZTE_CM_IC_ERROR("enable sd76930 otg failed ret = %d\n", ret);
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}
	/* read val*/

	if (sd76930_read(info, SD76930_REG_0, &reg_val)) {
		ZTE_CM_IC_ERROR("failed to get sd76930 otg status\n");
	} else {
		ZTE_CM_IC_INFO("reg:0x00 val:0x%x\n", reg_val);
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SD76930_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SD76930_OTG_VALID_MS));
	ZTE_CM_IC_INFO("exit!\n");
	return 0;
}

static int sd76930_charger_disable_otg(struct regulator_dev *dev)
{
	struct sd76930_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!");
		return -EINVAL;
	}

	ZTE_CM_IC_INFO("into\n");

	info->otg_enable = false;
	info->need_reinit = true;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	/*disable otg*/
	ret = sd76930_update_bits(info, SD76930_REG_0,
				   SD76930_REG_EN_OTG_MASK,
				   0);
	if (ret) {
		ZTE_CM_IC_ERROR("disable otg failed\n");
		return ret;
	}
	/*enable charger*/
	ret = sd76930_update_bits(info, SD76930_REG_0,
				   SD76930_REG_CHG_EN_MASK,
				   SD76930_REG_CHG_EN_MASK);
	if (ret) {
		ZTE_CM_IC_ERROR("enable sd76930 charger failed ret = %d\n", ret);
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		if (ret)
			ZTE_CM_IC_ERROR("enable BC1.2 failed\n");
	}

	return ret;

}

static int sd76930_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sd76930_charger_info *info = rdev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_INFO("otg_is_enabled %d!\n", info->otg_enable);

	return info->otg_enable;
}

static const struct regulator_ops sd76930_charger_vbus_ops = {
	.enable = sd76930_charger_enable_otg,
	.disable = sd76930_charger_disable_otg,
	.is_enabled = sd76930_charger_vbus_is_enabled,
};

static const struct regulator_desc sd76930_charger_vbus_desc = {
	.name = "charger_otg_vbus",
	.of_match = "charger_otg_vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sd76930_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
sd76930_charger_register_vbus_regulator(struct sd76930_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sd76930_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		ZTE_CM_IC_ERROR("Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
sd76930_charger_register_vbus_regulator(struct sd76930_charger_info *info)
{
	return 0;
}
#endif


static int sd76930_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sd76930_charger_info *info =
		container_of(nb, struct sd76930_charger_info, usb_notify);

	ZTE_CM_IC_INFO("sd76930_charger_usb_change: %d\n", limit);

	info->limit = limit;

#ifdef CONFIG_VENDOR_SQC_CHARGER
	sqc_notify_daemon_changed(SQC_NOTIFY_USB,
					SQC_NOTIFY_USB_STATUS_CHANGED, !!limit);
#endif

	return NOTIFY_OK;
}


static int sd76930_charger_get_recharging_vol(struct sd76930_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sd76930_read(info, SD76930_REG_2, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & SD76930_REG_VRCHG_MASK ) >> SD76930_REG_VRCHG_SHIFT;
	
    switch(reg_val) {
        case 0:
            *vol = 128;
			break;
        case 1:
            *vol = 256;
			break;
        default:
		    ZTE_CM_IC_ERROR("Read termina_vol failed,default is 128mV\n");
            *vol = 128;	
	}

    return 0;
}

static int sd76930_charger_get_termina_cur(struct sd76930_charger_info *info, u32 *cur)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sd76930_read(info, SD76930_REG_8, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & SD76930_REG_ITERM_MASK) >> SD76930_REG_ITERM_SHIFT;

    *cur = (reg_val * SD76930_REG_ITERM_LSB + SD76930_REG_ITERM_BASE) * 1000;

    return 0;
}

static int sd76930_charger_get_termina_vol(struct sd76930_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sd76930_read(info, SD76930_REG_2, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & SD76930_REG_VBAT_REG_MASK) >> SD76930_REG_VBAT_REG_SHIFT;

    *vol = (reg_val * SD76930_REG_VBAT_REG_LSB + SD76930_REG_VBAT_REG_BASE) * 1000;

    return 0;
}

static void sd76930_print_regs(struct sd76930_charger_info *info, char *buffer, unsigned int len)
{
	int i = 0;
	u8 value[SD76930_REG_NUM] = {0};
	char temp = 0;

	for (i = 0; i < ARRAY_SIZE(value); i++) {
		sd76930_read(info, reg_tab[i].addr, &(value[i]));
		if (i == ARRAY_SIZE(value) - 1) {
			ZTE_CM_IC_INFO("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,\n",
				value[0], value[1], value[2], value[3], value[4], value[5],
				value[6], value[7], value[8], value[9], value[10], value[11],
				value[12], value[13], value[14], value[15]);
		}
	}

	temp = (value[5] & GENMASK(4, 3)) >> 3;

	switch (temp) {
	case 0:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: NOT CHARGING, ");
		break;
	case 1:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: PRE CHARGING, ");
		break;
	case 2:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: FAST CHARGING, ");
		break;
	case 3:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: CHARGER TERMINATION, ");
		break;
	default:
		break;
	}

	if (value[6] & BIT(7)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "WT_FAULT: 1, ");
	}

	temp = (value[6] & GENMASK(5, 4)) >> 4;
	switch (temp) {
	case 0:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHRG_FAULT: NORMAL, ");
		break;
	case 1:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHRG_FAULT: VBUS POOR, ");
		break;
	case 2:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHRG_FAULT: VBUS OVP, ");
		break;
	case 3:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHRG_FAULT: TIMER EXP, ");
		break;
	default:
		break;
	}

	if (value[6] & BIT(3)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "BATOVP: 1, ");
	}

	if (value[5] & BIT(6)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 1, ");
		info->chg_status |= BIT(SQC_CHG_STATUS_AICL);
	} else {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 0, ");
		info->chg_status &= ~ BIT(SQC_CHG_STATUS_AICL);
	}

	if (value[5] & BIT(5)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "IINDPM: 1, ");
	}
}

static int sd76930_charger_dump_register(struct sd76930_charger_info *info)
{
	int usb_icl = 0, fcc = 0, fcv = 0, topoff = 0, recharge_voltage = 0;
	char buffer[512] = {0, };

	sd76930_charger_get_termina_vol(info, &fcv);

	sd76930_charger_get_input_limit_current(info, &usb_icl);

	sd76930_charger_get_current(info, &fcc);

	sd76930_charger_get_termina_cur(info, &topoff);

	sd76930_charger_get_recharging_vol(info, &recharge_voltage);
	
	sd76930_print_regs(info, buffer, sizeof(buffer));

	ZTE_CM_IC_INFO("charging[%d], fcv[%d], usb_icl[%d], fcc[%d], topoff[%d], rechg_volt[%d], %s",
				info->charging, fcv / 1000, usb_icl / 1000, fcc / 1000,
				topoff / 1000, recharge_voltage / 1000, buffer);
	return 0;
}

static void sd76930_charger_detect_status(struct sd76930_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	ZTE_CM_IC_INFO("charger_detect_status %d", info->usb_phy->chg_state);
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	ZTE_CM_IC_INFO("limit %d", min);
	return;
}


static int sd76930_charger_feed_watchdog(struct sd76930_charger_info *info)
{
	int ret;
	u8 reg_val;

	//WD_RST
	sd76930_update_bits(info, SD76930_REG_1, SD76930_REG_WD_REST_MASK,
				   SD76930_REG_WD_REST_MASK);

	ret = sd76930_read(info, SD76930_REG_1, &reg_val);
	if (ret)
		ZTE_CM_IC_ERROR("read REG_1 failed ret = %d\n", ret);

	ZTE_CM_IC_INFO("SD76930_REG_1 =0X%2X.\n", reg_val);

	return ret;
}

static int32_t sd76930_get_charger_en(struct sd76930_charger_info *info, uint32_t *chg_enable)
{
	uint8_t data_reg = 0;
	int32_t ret = 0;

	ret = sd76930_read(info, SD76930_REG_0, &data_reg);
	if(ret) {
		ZTE_CM_IC_ERROR("Failed to read charger_en status!\n");
		return ret;
	}

	data_reg = (data_reg & SD76930_REG_CHG_EN_MASK) >> SD76930_REG_CHG_EN_SHIFT;

	*chg_enable = !!data_reg;

	return 0;
}

static int32_t sd76930_set_wd_timer(struct sd76930_charger_info *info, uint32_t second)
{
	uint8_t data_reg = 0;

	ZTE_CM_IC_INFO("second= %d\n", second);

	if(second < 40)		//second < 40s, disable wdt
		data_reg = 0x00;
	else
		data_reg = 0x01;	//second >= 40, set 40s

	return sd76930_update_bits(info, SD76930_REG_4, SD76930_REG_WDOG_EN_MASK, data_reg << SD76930_REG_WDOG_EN_SHIFT);
}

static int sd76930_charger_set_termina_cur(struct sd76930_charger_info *info, u32 cur)
{
    u8 reg_val = 0;

    if (cur <= SD76930_REG_ITERM_MIN)
        cur = SD76930_REG_ITERM_MIN;
    else if (cur >= SD76930_REG_ITERM_MAX)
        cur = SD76930_REG_ITERM_MAX;
    reg_val = (cur - SD76930_REG_ITERM_BASE) / SD76930_REG_ITERM_LSB;

    return sd76930_update_bits(info, SD76930_REG_8, SD76930_REG_ITERM_MASK, reg_val << SD76930_REG_ITERM_SHIFT);
}

 
 static int sd76930_charger_set_vindpm(struct sd76930_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol < SD76930_REG_VINDPM_MIN)
        vol = SD76930_REG_VINDPM_MIN;
    else if (vol > SD76930_REG_VINDPM_MAX)
        vol = SD76930_REG_VINDPM_MAX;
    reg_val = (vol - SD76930_REG_VINDPM_BASE) / SD76930_REG_VINDPM_LSB;

    return sd76930_update_bits(info, SD76930_REG_7, SD76930_REG_VINDPM_MASK, reg_val << SD76930_REG_VINDPM_SHIFT);
}

static int sd76930_charger_set_recharging_vol(struct sd76930_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol <= 255000)
        reg_val = 0x00;
	else
        reg_val = 0x01;

    return sd76930_update_bits(info, SD76930_REG_2, SD76930_REG_VRCHG_MASK, reg_val << SD76930_REG_VRCHG_SHIFT);
}

static int sd76930_charger_set_hiz(struct sd76930_charger_info *info, u32 en_hiz)
{
    u8 reg_val = 0;

    if (en_hiz)
        reg_val = SD76930_EN_HIZ;
    else
        reg_val = SD76930_EXIT_HIZ;


    return sd76930_update_bits(info, SD76930_REG_0, SD76930_REG_EN_HIZ_MASK, reg_val << SD76930_REG_EN_HIZ_SHIFT);
}

static int sd76930_charger_get_hiz(struct sd76930_charger_info *info, u32 *en_hiz)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sd76930_read(info, SD76930_REG_0, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & SD76930_REG_EN_HIZ_MASK ) >> SD76930_REG_EN_HIZ_SHIFT;

    *en_hiz = reg_val ? SD76930_EN_HIZ : SD76930_EXIT_HIZ;

    return 0;
}


#ifdef CONFIG_ZTE_POWER_SUPPLY_COMMON
#ifndef CONFIG_FAST_CHARGER_SC27XX
static int zte_sqc_set_prop_by_name(const char *name, enum zte_power_supply_property psp, int data)
{
	struct zte_power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		ZTE_CM_IC_INFO("sd76930:psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = zte_power_supply_get_by_name(name);
	if (!psy) {
		ZTE_CM_IC_INFO("sd76930:get %s psy failed!!\n", name);
		goto failed_loop;
	}

	val.intval = data;

	rc = zte_power_supply_set_property(psy,
				psp, &val);
	if (rc < 0) {
		ZTE_CM_IC_INFO("Failed to set %s property:%d rc=%d\n", name, psp, rc);
		return rc;
	}

	zte_power_supply_put(psy);

	return 0;

failed_loop:
	return -EINVAL;
}
#endif

static int sqc_mp_get_chg_type(void *arg, unsigned int *chg_type)
{
	arg = arg ? arg : NULL;

	*chg_type = SQC_PMIC_TYPE_BUCK_5A;

	return 0;
}

static int sqc_mp_get_chg_status(void *arg, unsigned int *charing_status)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

	*charing_status = info->chg_status;

	return 0;
}

static int sqc_mp_set_enable_chging(void *arg, unsigned int en)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

	if (en && info->enable_powerptah) {
		ZTE_CM_IC_INFO("powerptah enabled, force disable chging\n");
		en = false;
	}

	if (en) {
		sd76930_set_wd_timer(info, 40);
		sd76930_charger_feed_watchdog(info);

		sd76930_charger_start_charge(info);

		sd76930_charger_set_vindpm(info, 4600);

		info->charging = !!en;

		sd76930_charger_dump_register(info);
	} else {
		sd76930_set_wd_timer(info, 0);
		sd76930_charger_stop_charge(info);

		sd76930_charger_dump_register(info);
		info->charging = !!en;
	}

	ZTE_CM_IC_INFO("%d\n", en);

	return 0;
}

static int sqc_mp_get_enable_chging(void *arg, unsigned int *en)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_get_charger_en(info, en);

	ZTE_CM_IC_INFO("%d\n", *en);

	return ret;
}

static int sqc_mp_get_ichg(void *arg, u32 *ichg_ma)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_current(info, ichg_ma);

	ZTE_CM_IC_INFO("%d\n", *ichg_ma);

	return ret;
}

static int sqc_mp_set_ichg(void *arg, u32 mA)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_set_current(info, mA * 1000);

	ZTE_CM_IC_INFO("%d mA\n", mA);

	return ret;
}

static int sqc_mp_set_ieoc(void *arg, u32 mA)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_set_termina_cur(info, mA);

	ZTE_CM_IC_INFO("%d mA\n", mA);

	return ret;
}

static int sqc_mp_get_ieoc(void *arg, u32 *ieoc)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_termina_cur(info, ieoc);

	ZTE_CM_IC_INFO("%d\n", *ieoc);

	return ret;
}

static int sqc_mp_get_aicr(void *arg, u32 *aicr_ma)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_input_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("%d\n", *aicr_ma);

	return ret;
}

static int sqc_mp_set_aicr(void *arg, u32 aicr_ma)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_set_input_limit_current(info, aicr_ma * 1000);

	ZTE_CM_IC_INFO("%d\n", aicr_ma);

	return ret;
}

static int sqc_mp_get_cv(void *arg, u32 *cv_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("%d\n", *cv_mv);

	return ret;
}

static int sqc_mp_set_cv(void *arg, u32 cv_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_set_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("%d\n", cv_mv);

	return ret;
}

static int sqc_mp_get_rechg_voltage(void *arg, u32 *rechg_volt_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("%d\n", *rechg_volt_mv);

	return ret;
}

static int sqc_mp_set_rechg_voltage(void *arg, u32 rechg_volt_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_set_recharging_vol(info, rechg_volt_mv * 1000);

	ZTE_CM_IC_INFO("%d\n", rechg_volt_mv);

	return ret;
}

static int sqc_mp_ovp_volt_get(void *arg, u32 *ac_ovp_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

    *ac_ovp_mv = info->vbus_ovp_mv;

    ZTE_CM_IC_INFO("%dmV\n", *ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_ovp_volt_set(void *arg, u32 ac_ovp_mv)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

	info->vbus_ovp_mv = ac_ovp_mv;
	
	ZTE_CM_IC_INFO("%d mV\n", info->vbus_ovp_mv);
	
	sd76930_charger_set_ovp(info, info->vbus_ovp_mv);

	return 0;
}

static int sqc_mp_get_vbat(void *arg, u32 *mV)
{
	union power_supply_propval batt_vol_uv;
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SD76930_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SD76930_FGU_NAME);
		return -ENODEV;
	}

	ret1 = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &batt_vol_uv);

	power_supply_put(fuel_gauge);
	if (ret1) {
		ZTE_CM_IC_ERROR("get POWER_SUPPLY_PROP_VOLTAGE_NOW failed!\n");
		return ret1;
	}

	*mV = batt_vol_uv.intval / 1000;

    ZTE_CM_IC_INFO("%dmV\n", *mV);

	return 0;
}

static int sqc_mp_get_ibat(void *arg, u32 *mA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SD76930_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SD76930_FGU_NAME);
		return -ENODEV;
	}

	ret1 = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);

	power_supply_put(fuel_gauge);
	if (ret1) {
		ZTE_CM_IC_ERROR("get POWER_SUPPLY_PROP_CURRENT_NOW failed!\n");
		return ret1;
	}

	*mA = val.intval / 1000;

	ZTE_CM_IC_INFO("%d mA\n", *mA);

	return 0;
}

static int sqc_mp_get_vbus(void *arg, u32 *mV)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;
	union power_supply_propval val = {};
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SD76930_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SD76930_FGU_NAME);
		return -ENODEV;
	}

	ret1 = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);

	power_supply_put(fuel_gauge);
	if (ret1) {
		ZTE_CM_IC_ERROR("get POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE failed!\n");
		return ret1;
	}

	*mV = val.intval / 1000;

	ZTE_CM_IC_INFO("adc:%d mV, vbus_ovp_mv:%d mV\n", *mV, info->vbus_ovp_mv);

	return 0;
}

static int sqc_mp_get_ibus(void *arg, u32 *mA)
{
	arg = arg ? arg : NULL;

	*mA = 0;

	return 0;
}

static int sqc_enable_powerpath_set(void *arg, unsigned int enabled)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

	info->enable_powerptah = !!enabled;

	return 0;
}

static int sqc_enable_powerpath_get(void *arg, unsigned int *enabled)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)arg;

	*enabled = info->enable_powerptah;

	return 0;
}

static int sqc_enable_hiz_set(void *arg, unsigned int enable)
{
	struct sd76930_charger_info *chg_dev = (struct sd76930_charger_info *)arg;

	sd76930_charger_set_hiz(chg_dev, !!enable);

	return 0;
}

static int sqc_enable_hiz_get(void *arg, unsigned int *enable)
{
	struct sd76930_charger_info *chg_dev = (struct sd76930_charger_info *)arg;
	int ret = 0;

	ret = sd76930_charger_get_hiz(chg_dev, enable);

	return ret;
}

static struct sqc_pmic_chg_ops sd76930_sqc_chg_ops = {

	.init_pmic_charger = NULL,

	.get_chg_type = sqc_mp_get_chg_type,
	.get_chg_status = sqc_mp_get_chg_status,


	.chg_enable = sqc_mp_set_enable_chging,
	.chg_enable_get = sqc_mp_get_enable_chging,

	.set_chging_fcv = sqc_mp_set_cv,
	.get_chging_fcv = sqc_mp_get_cv,

	.set_chging_fcc = sqc_mp_set_ichg,
	.get_chging_fcc = sqc_mp_get_ichg,

	.set_chging_icl = sqc_mp_set_aicr,
	.get_chging_icl = sqc_mp_get_aicr,

	.set_chging_topoff = sqc_mp_set_ieoc,
	.get_chging_topoff = sqc_mp_get_ieoc,

	.set_rechg_volt = sqc_mp_set_rechg_voltage,
	.get_rechg_volt = sqc_mp_get_rechg_voltage,

	.ac_ovp_volt_set = sqc_mp_ovp_volt_set,
	.ac_ovp_volt_get = sqc_mp_ovp_volt_get,

	.batt_ibat_get = sqc_mp_get_ibat,
	.batt_vbat_get = sqc_mp_get_vbat,

	.usb_ibus_get = sqc_mp_get_ibus,
	.usb_vbus_get = sqc_mp_get_vbus,

	.enable_path_set = sqc_enable_powerpath_set,
	.enable_path_get = sqc_enable_powerpath_get,

	.enable_hiz_set = sqc_enable_hiz_set,
	.enable_hiz_get = sqc_enable_hiz_get,
};
#ifndef CONFIG_FAST_CHARGER_SC27XX
extern struct sqc_bc1d2_proto_ops sqc_bc1d2_proto_node;
static int sqc_chg_type = SQC_NONE_TYPE;

static int sqc_bc1d2_get_charger_type(unsigned int *chg_type)
{
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)sqc_bc1d2_proto_node.arg;

	if (!info || !info->usb_phy) {
		ZTE_CM_IC_ERROR("[SQC-HW]: info is null\n");
		*chg_type = SQC_NONE_TYPE;
		return 0;
	}

	if (!info->limit) {
		*chg_type = SQC_NONE_TYPE;
		goto out_loop;
	}

  	switch (info->usb_phy->chg_type) {
	case SDP_TYPE:
		*chg_type = SQC_SDP_TYPE;
		break;
	case DCP_TYPE:
		*chg_type = SQC_DCP_TYPE;
		break;
	case CDP_TYPE:
		*chg_type = SQC_CDP_TYPE;
		break;
	case ACA_TYPE:
	default:
		*chg_type = SQC_FLOAT_TYPE;
	}

	ZTE_CM_IC_INFO("sqc_chg_type: %d, chg_type: %d\n", sqc_chg_type, *chg_type);

	if ((sqc_chg_type == SQC_SLEEP_MODE_TYPE)
			&& (*chg_type == SQC_NONE_TYPE)) {
		sqc_chg_type = *chg_type;
		zte_sqc_set_prop_by_name("zte_battery", POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, 1);
	} else if (sqc_chg_type == SQC_SLEEP_MODE_TYPE) {
		*chg_type = sqc_chg_type;
	}

out_loop:
	ZTE_CM_IC_INFO("[SQC-HW]: limit: %d, sprd_type: %d, chg_type: %d\n",
		info->limit, info->usb_phy->chg_type, *chg_type);

	return 0;
}


struct sqc_bc1d2_proto_ops sqc_bc1d2_proto_node = {
	.status_init = NULL,
	.status_remove = NULL,
	.get_charger_type = sqc_bc1d2_get_charger_type,
	.set_charger_type = NULL,
	.get_protocol_status = NULL,
	.get_chip_vendor_id = NULL,
	.set_qc3d0_dp = NULL,
	.set_qc3d0_dm = NULL,
	.set_qc3d0_plus_dp = NULL,
	.set_qc3d0_plus_dm = NULL,
};

int sqc_sleep_node_set(const char *val, const void *arg)
{
	int sleep_mode_enable = 0, ret = 0;
	struct sd76930_charger_info *info = (struct sd76930_charger_info *)sqc_bc1d2_proto_node.arg;

	sscanf(val, "%d", &sleep_mode_enable);

	ZTE_CM_IC_INFO("sleep_mode_enable = %d\n", sleep_mode_enable);

	if (sleep_mode_enable) {
		if (sqc_chg_type != SQC_SLEEP_MODE_TYPE) {
			ZTE_CM_IC_INFO("sleep on status");

			/*disabel sqc-daemon*/
			sqc_chg_type = SQC_SLEEP_MODE_TYPE;
			sqc_notify_daemon_changed(SQC_NOTIFY_USB, SQC_NOTIFY_USB_STATUS_CHANGED, 1);

            ret = sd76930_update_bits(info, SD76930_REG_4, SD76930_REG_WDOG_EN_MASK, 0 << SD76930_REG_WDOG_EN_SHIFT);
            if (ret) {
                ZTE_CM_IC_ERROR("Failed to disable watchdog\n");
            }
			/*enter sleep mode*/
			zte_sqc_set_prop_by_name("zte_battery", POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, 0);

		}
	} else {
		if (sqc_chg_type != SQC_SLEEP_MODE_TYPE) {
			ZTE_CM_IC_INFO("sleep off status");
			sqc_chg_type = SQC_NONE_TYPE;
		}
	}

	return 0;
}

int sqc_sleep_node_get(char *val, const void *arg)
{
	int sleep_mode = 0;

	if (sqc_chg_type == SQC_SLEEP_MODE_TYPE)
		sleep_mode = 1;

	return snprintf(val, PAGE_SIZE, "%u", sleep_mode);
}

static struct zte_misc_ops qc3dp_sleep_mode_node = {
	.node_name = "qc3dp_sleep_mode",
	.set = sqc_sleep_node_set,
	.get = sqc_sleep_node_get,
	.free = NULL,
	.arg = NULL,
};
#endif
#endif


static int sd76930_chip_name_show(struct seq_file *m, void *v)
{
	struct sd76930_charger_info *pinfo = m->private;

	if (pinfo) {
		seq_printf(m, "sd76930\n");
	} else {
		seq_printf(m, "unknow\n");
	}
	return 0;
}

static int sd76930_chip_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, sd76930_chip_name_show, PDE_DATA(inode));
}

static const struct file_operations sd76930_chip_name_node = {
	.owner = THIS_MODULE,
	.open = sd76930_chip_name_open,
	.read = seq_read,
	.llseek = seq_lseek,
};


static int32_t sd76930_get_chip_id(struct sd76930_charger_info *chip)
{
	uint8_t reg_value = 0;
	int32_t ret = 0;

	ret = sd76930_read(chip, SD76930_REG_3, &reg_value);
	if(ret < 0)
		return ret;

	if (ret < 0) {
		ZTE_CM_IC_INFO(" read id failed, ret = %d\n", ret);
		return ret;
	} else if((reg_value & 0xff) != 0x20) {
		ZTE_CM_IC_INFO("chip id = 0x%02X\n", (reg_value & 0xff));
		return -1;
	} else {
		ZTE_CM_IC_INFO(" id correct,found SD76930!\n");
	}

	return 0;
}

static int sd76930_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sd76930_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sqc_pmic_chg_ops *sqc_ops = NULL;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		ZTE_CM_IC_ERROR("No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	ret = sd76930_get_chip_id(info);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("get id failed\n");
		return -ENODEV;
	}

	alarm_init(&info->wdg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(info->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		ZTE_CM_IC_ERROR("failed to find USB phy\n");
	}
	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		ZTE_CM_IC_ERROR("failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	info->use_typec_extcon = device_property_read_bool(info->dev, "use-typec-extcon");

	info->host_status_check = device_property_read_bool(info->dev, "host-status-check");
	
	ret = sd76930_charger_register_vbus_regulator(info);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register vbus regulator.\n");
		return -EPROBE_DEFER;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		ZTE_CM_IC_ERROR("unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to get charger_detect\n");
		return -EINVAL;
	}

	info->gpiod = devm_gpiod_get(dev, "chg-enable", ENABLE_CHARGE);
	if (IS_ERR(info->gpiod)) {
		ZTE_CM_IC_ERROR("failed to get enable gpio\n");
		//return PTR_ERR(info->gpiod);
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to get charger_pd reg\n");
		return ret;
	}

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = SD76930_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = SD76930_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = SD76930_DISABLE_PIN_MASK_2720;
	else {
		ZTE_CM_IC_ERROR("failed to get charger_pd mask\n");
		return -EINVAL;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		ZTE_CM_IC_ERROR("unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		ZTE_CM_IC_ERROR("unable to get pmic regmap device\n");
		return -ENODEV;
	}

	info->usb_notify.notifier_call = sd76930_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register notifier:%d\n", ret);
		goto GET_USB_PHY_FAILED;
	}
	proc_create_data("driver/chg_name", 0664, NULL, &sd76930_chip_name_node, info);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sd76930_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		ZTE_CM_IC_ERROR("failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = sd76930_charger_hw_init(info);
	if (ret)
		goto USB_REGISTER_NOTIFIER_FAILED;

	sd76930_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);

	INIT_DELAYED_WORK(&info->otg_work, sd76930_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  sd76930_charger_feed_watchdog_work);

	/*zte add to get limit value*/
	sd76930_charger_detect_status(info);
	sqc_ops = kzalloc(sizeof(struct sqc_pmic_chg_ops), GFP_KERNEL);
	memcpy(sqc_ops, &sd76930_sqc_chg_ops, sizeof(struct sqc_pmic_chg_ops));
	sqc_ops->arg = (void *)info;

	ret = sqc_hal_charger_register(sqc_ops, SQC_CHARGER_PRIMARY);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("register sqc hal fail(%d)\n", ret);
	}

#ifndef CONFIG_FAST_CHARGER_SC27XX
	sqc_bc1d2_proto_node.arg = (void *)info;
	sqc_hal_bc1d2_register(&sqc_bc1d2_proto_node);

	zte_misc_register_callback(&qc3dp_sleep_mode_node, info);
#endif

	info->sd_wake_lock = wakeup_source_register(info->dev, "sd76930_wake_lock");
	if (!info->sd_wake_lock) {
		ZTE_CM_IC_ERROR("wakelock register failed!\n");
		goto USB_REGISTER_NOTIFIER_FAILED;
	}


	info->init_finished = 1;

	ZTE_CM_IC_INFO("successful!\n");

	return 0;
USB_REGISTER_NOTIFIER_FAILED:
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
GET_USB_PHY_FAILED:
	mutex_destroy(&info->lock);
	if (info)
		devm_kfree(info->dev, info);

	return -EPROBE_DEFER;
}

static void sd76930_charger_shutdown(struct i2c_client *client)
{
	struct sd76930_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->need_reinit = true;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sd76930_update_bits(info, SD76930_REG_0,
					  SD76930_REG_EN_OTG_MASK,
					  0);
		if (ret)
			ZTE_CM_IC_ERROR("disable sd76930 otg failed ret = %d\n", ret);

		ZTE_CM_IC_INFO("disable otg func when shutdown\n");
		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			ZTE_CM_IC_ERROR("enable charger detection function failed ret = %d\n", ret);
	}
}

static int sd76930_charger_remove(struct i2c_client *client)
{
	struct sd76930_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sd76930_charger_alarm_prepare(struct device *dev)
{
	struct sd76930_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;

	if (!info) {
		ZTE_CM_IC_ERROR("info is null!\n");
		return 0;
	}

	if (!info->otg_enable)
		return 0;

	now = ktime_get_boottime();
	add = ktime_set(SD76930_WDG_TIMER_S, 0);
	alarm_start(&info->wdg_timer, ktime_add(now, add));
	return 0;
}

static void sd76930_charger_alarm_complete(struct device *dev)
{
	struct sd76930_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return;
	}

	if (!info->otg_enable)
		return;

	alarm_cancel(&info->wdg_timer);
}

static int sd76930_charger_suspend(struct device *dev)
{
	struct sd76930_charger_info *info = dev_get_drvdata(dev);

	if (info->otg_enable || info->is_charger_online)
		/* feed watchdog first before suspend */
		sd76930_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	return 0;
}

static int sd76930_charger_resume(struct device *dev)
{
	struct sd76930_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online)
		/* feed watchdog first before suspend */
		sd76930_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops sd76930_charger_pm_ops = {
	.prepare = sd76930_charger_alarm_prepare,
	.complete = sd76930_charger_alarm_complete,
	SET_SYSTEM_SLEEP_PM_OPS(sd76930_charger_suspend,
				sd76930_charger_resume)
};

static const struct i2c_device_id sd76930_i2c_id[] = {
	{"sd76930_chg", 0},
	{}
};

static const struct of_device_id sd76930_charger_of_match[] = {
	{ .compatible = "bigmtech,sd76930", },
	{ }
};

MODULE_DEVICE_TABLE(of, sd76930_charger_of_match);

static struct i2c_driver sd76930_charger_driver = {
	.driver = {
		.name = "sd76930_chg",
		.of_match_table = sd76930_charger_of_match,
		.pm = &sd76930_charger_pm_ops,
	},
	.probe = sd76930_charger_probe,
	.shutdown = sd76930_charger_shutdown,
	.remove = sd76930_charger_remove,
	.id_table = sd76930_i2c_id,
};

module_i2c_driver(sd76930_charger_driver);
MODULE_DESCRIPTION("SD76930 Charger Driver");
MODULE_LICENSE("GPL v2");
