/*
 * TI SGM41510 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define CHARGER_IC_TAG   "[ZTE_LDD_CHARGE][CHARGER_IC][sgm41510]"

#define ZTE_CM_IC_INFO(fmt, args...) {pr_info(CHARGER_IC_TAG"[Info]%s: "fmt, __func__, ##args); }

#define ZTE_CM_IC_ERROR(fmt, args...) {pr_err(CHARGER_IC_TAG"[Error]%s: "fmt, __func__, ##args); }

#define ZTE_CM_IC_DEBUG(fmt, args...) {pr_debug(CHARGER_IC_TAG"[Debug]%s: "fmt, __func__, ##args); }


#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>

#include <sqc_common.h>
#include <vendor/common/zte_misc.h>
int sqc_notify_daemon_changed(int chg_id, int msg_type, int msg_val);

#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
#include <vendor/comdef/zlog_common_base.h>
#endif

#define SGM41510_REG_00				0x00
#define SGM41510_REG_01				0x01
#define SGM41510_REG_02				0x02
#define SGM41510_REG_03				0x03
#define SGM41510_REG_04				0x04
#define SGM41510_REG_05				0x05
#define SGM41510_REG_06				0x06
#define SGM41510_REG_07				0x07
#define SGM41510_REG_08				0x08
#define SGM41510_REG_09				0x09
#define SGM41510_REG_0A				0x0a
#define SGM41510_REG_0B				0x0b
#define SGM41510_REG_0C				0x0c
#define SGM41510_REG_0D				0x0d
#define SGM41510_REG_0E				0x0e
#define SGM41510_REG_0F				0x0f
#define SGM41510_REG_10				0x10
#define SGM41510_REG_11				0x11
#define SGM41510_REG_12				0x12
#define SGM41510_REG_13				0x13
#define SGM41510_REG_14				0x14
#define SGM41510_REG_15				0x15
#define SGM41510_REG_NUM				22

/* Register 0x00 */
#define REG00_ENHIZ_MASK			0x80
#define REG00_ENHIZ_SHIFT			7
#define REG00_EN_HIZ				1
#define REG00_EXIT_HIZ				0
#define REG00_EN_ILIM_MASK			0x40
#define REG00_EN_ILIM_SHIFT			6
#define REG00_IINLIM_MASK			0x3f
#define REG00_IINLIM_SHIFT			0

/* Register 0x01
#define REG01_BHOT_MASK				0xc0
#define REG01_BHOT_SHIFT			6
#define REG01_BCOLD_MASK			0x20
#define REG01_BCOLD_SHIFT			5
#define REG01_VINDPM_OS_MASK			0x1f
#define REG01_VINDPM_OS_SHIFT			0
*/

/* Register 0x02*/
#define REG02_CONV_START_MASK			0x80
#define REG02_CONV_START_SHIFT			7
//#define REG02_CONV_RATE_MASK			0x40
//#define REG02_CONV_RATE_SHIFT			6
#define REG02_BOOST_FREQ_MASK			0x20
#define REG02_BOOST_FREQ_SHIFT			5
//#define REG02_ICO_EN_MASK			0x10
//#define REG02_ICO_EN_SHIFT			4
//#define REG02_HVDCP_EN_MASK			0x08
//#define REG02_HVDCP_EN_SHIFT			3
//#define REG02_MAXC_EN_MASK			0x04
//#define REG02_MAXC_EN_SHIFT			2
#define REG02_FORCE_DPDM_MASK			0x02
#define REG02_FORCE_DPDM_SHIFT			1
#define REG02_AUTO_DPDM_EN_MASK			0x01
#define REG02_AUTO_DPDM_EN_SHIFT		0

/* Register 0x03 */
#define REG03_BAT_LOADEN_MASK			0x80
#define REG03_BAT_LOADEN_SHIFT			7
#define REG03_WDT_RESET_MASK			0x40
#define REG03_WDT_RESET_SHIFT			6
#define REG03_OTG_CONFIG_MASK			0x20
#define REG03_OTG_CONFIG_SHIFT			5
#define REG03_CHG_CONFIG_MASK			0x10
#define REG03_CHG_CONFIG_SHIFT			4
#define REG03_SYS_MINV_MASK			    0x0e
#define REG03_SYS_MINV_SHIFT			1

/* Register 0x04*/
#define REG04_EN_PUMPX_MASK			0x80
#define REG04_EN_PUMPX_SHIFT			7
#define REG04_ICHG_MASK				0x7f
#define REG04_ICHG_SHIFT			0

/* Register 0x05*/
#define REG05_IPRECHG_MASK			0xf0
#define REG05_IPRECHG_SHIFT			4
#define REG05_ITERM_MASK			0x0f
#define REG05_ITERM_SHIFT			0

/* Register 0x06*/
#define REG06_VREG_MASK				0xfc
#define REG06_VREG_SHIFT			2
#define REG06_BATLOWV_MASK			0x02
#define REG06_BATLOWV_SHIFT			1
#define REG06_VRECHG_MASK			0x01
#define REG06_VRECHG_SHIFT			0

/* Register 0x07*/
#define REG07_EN_TERM_MASK			0x80
#define REG07_EN_TERM_SHIFT			7
#define REG07_STAT_DIS_MASK			0x40
#define REG07_STAT_DIS_SHIFT			6
#define REG07_WDT_MASK				0x30
#define REG07_WDT_SHIFT				4
#define REG07_EN_TIMER_MASK			0x08
#define REG07_EN_TIMER_SHIFT			3
#define REG07_CHG_TIMER_MASK			0x06
#define REG07_CHG_TIMER_SHIFT			1
//#define REG07_JEITA_ISET_MASK			0x01
//#define REG07_JEITA_ISET_SHIFT			0

/* Register 0x08*/
#define REG08_IR_COMP_MASK			0xe0
#define REG08_IR_COMP_SHIFT			5
#define REG08_VCLAMP_MASK			0x1c
#define REG08_VCLAMP_SHIFT			2
#define REG08_TREG_MASK				0x03
#define REG08_TREG_SHIFT			2

/* Register 0x09*/
//#define REG09_FORCE_ICO_MASK			0x80
//#define REG09_FORCE_ICO_SHIFT			7
#define REG09_TMR2X_EN_MASK			0x40
#define REG09_TMR2X_EN_SHIFT			6
#define REG09_BATFET_DIS_MASK			0x20
#define REG09_BATFET_DIS_SHIFT			5
//#define REG09_JEITA_VSET_MASK			0x10
//#define REG09_JEITA_VSET_SHIFT			4
#define REG09_BATFET_DLY_MASK			0x08
#define REG09_BATFET_DLY_SHIFT			3
#define REG09_BATFET_RST_EN_MASK		0x04
#define REG09_BATFET_RST_EN_SHIFT		2
#define REG09_PUMPX_UP_MASK			0x02
#define REG09_PUMPX_UP_SHIFT			1
#define REG09_PUMPX_DN_MASK			0x01
#define REG09_PUMPX_DN_SHIFT			0

/* Register 0x0A*/
#define REG0A_BOOSTV_MASK			0xf0
#define REG0A_BOOSTV_SHIFT			4
#define REG0A_BOOSTV_LIM_MASK			0x07
#define REG0A_BOOSTV_LIM_SHIFT			0

/* Register 0x0B*/
#define REG0B_VBUS_STAT_MASK			0xe0
#define REG0B_VBUS_STAT_SHIFT			5
#define REG0B_CHRG_STAT_MASK			0x18
#define REG0B_CHRG_STAT_SHIFT			3
#define REG0B_PG_STAT_MASK			0x04
#define REG0B_PG_STAT_SHIFT			2
#define REG0B_VSYS_STAT_MASK			0x01
#define REG0B_VSYS_STAT_SHIFT			0

/* Register 0x0C*/
#define REG0C_FAULT_WDT_MASK			0x80
#define REG0C_FAULT_WDT_SHIFT			7
#define REG0C_FAULT_BOOST_MASK			0x40
#define REG0C_FAULT_BOOST_SHIFT			6
#define REG0C_FAULT_CHRG_MASK			0x30
#define REG0C_FAULT_CHRG_SHIFT			4
#define REG0C_FAULT_BAT_MASK			0x08
#define REG0C_FAULT_BAT_SHIFT			3
//#define REG0C_FAULT_NTC_MASK			0x07
//#define REG0C_FAULT_NTC_SHIFT			0

/* Register 0x0D*/
#define REG0D_FORCE_VINDPM_MASK			0x80
#define REG0D_FORCE_VINDPM_SHIFT		7
#define REG0D_VINDPM_MASK			0x7f
#define REG0D_VINDPM_SHIFT			0

/* Register 0x0E*/
#define REG0E_THERM_STAT_MASK			0x80
#define REG0E_THERM_STAT_SHIFT			7
//#define REG0E_BATV_MASK				0x7f
//#define REG0E_BATV_SHIFT			0

/* Register 0x0F*/
//#define REG0F_SYSV_MASK				0x7f
//#define REG0F_SYSV_SHIFT			0

/* Register 0x10*/
//#define REG10_TSPCT_MASK			0x7f
//#define REG10_TSPCT_SHIFT			0

/* Register 0x11*/
#define REG11_VBUS_GD_MASK			0x80
#define REG11_VBUS_GD_SHIFT			7
//#define REG11_VBUSV_MASK			0x7f
//#define REG11_VBUSV_SHIFT			0

/* Register 0x12*/
//#define REG12_ICHGR_MASK			0x7f
//#define REG12_ICHGR_SHIFT			0

/* Register 0x13*/
#define REG13_VDPM_STAT_MASK			0x80
#define REG13_VDPM_STAT_SHIFT			7
#define REG13_IDPM_STAT_MASK			0x40
#define REG13_IDPM_STAT_SHIFT			6
//#define REG13_IDPM_LIM_MASK			0x3f
//#define REG13_IDPM_LIM_SHIFT			0

/* Register 0x14 */
#define REG14_REG_RESET_MASK			0x80
#define REG14_REG_RESET_SHIFT			7
//#define REG14_REG_ICO_OP_MASK			0x40
//#define REG14_REG_ICO_OP_SHIFT			6
#define REG14_PN_MASK				0x38
#define REG14_PN_SHIFT				3
//#define REG14_TS_PROFILE_MASK			0x04
//#define REG14_TS_PROFILE_SHIFT			2
#define REG14_DEV_REV_MASK			0x03
#define REG14_DEV_REV_SHIFT			0

/* Register 0x15*/
#define REG15_CM_OUT_MASK			 0xE0
#define REG15_CM_OUT_SHIFT			5
#define REG15_VBUS_OV_MASK			0x1C
#define REG15_VBUS_OV_SHIFT			2

#define REG00_HIZ_DISABLE			0
#define REG00_HIZ_ENABLE			1
#define REG00_EN_ILIM_DISABLE			0
#define REG00_EN_ILIM_ENABLE			1
#define REG00_IINLIM_OFFSET			100
#define REG00_IINLIM_STEP			100
#define REG00_IINLIM_MIN			100
#define REG00_IINLIM_MAX			4900

//#define REG01_BHOT_VBHOT1			0
//#define REG01_BHOT_VBHOT0			1
//#define REG01_BHOT_VBHOT2			2
//#define REG01_BHOT_DISABLE			3
//#define REG01_BHOT_VBCOLD0			0
//#define REG01_BHOT_VBCOLD1			1
//#define REG01_VINDPM_OS_OFFSET			0
//#define REG01_VINDPM_OS_STEP			100
//#define REG01_VINDPM_OS_MIN			0
//#define REG01_VINDPM_OS_MAX			3100

#define REG02_CONV_START_DISABLE		0
#define REG02_CONV_START_ENABLE			1

#define REG02_BOOST_FREQ_1p5M			0
#define REG02_BOOST_FREQ_500K			1

#define REG02_FORCE_DPDM_DISABLE		0
#define REG02_FORCE_DPDM_DENABLE		1
#define REG02_AUTO_DPDM_EN_DISABLE		0
#define REG02_AUTO_DPDM_EN_DENABLE		1

#define REG03_BAT_ENABLE			0
#define REG03_BAT_DISABLE			1
#define REG03_WDT_RESET				1
#define REG03_OTG_DISABLE			0
#define REG03_OTG_ENABLE			1
#define REG03_CHG_DISABLE			0
#define REG03_CHG_ENABLE			1
#define REG03_SYS_MINV_OFFSET			3000
#define REG03_SYS_MINV_STEP			100
#define REG03_SYS_MINV_MIN			3000
#define REG03_SYS_MINV_MAX			3700

#define REG04_EN_PUMPX_DISABLE			0
#define REG04_EN_PUMPX_ENABLE			1
#define REG04_ICHG_OFFSET			0
#define REG04_ICHG_STEP				64
#define REG04_ICHG_MIN				0
#define REG04_ICHG_MAX				5056

#define REG05_IPRECHG_OFFSET			64
#define REG05_IPRECHG_STEP			64
#define REG05_IPRECHG_MIN			64
#define REG05_IPRECHG_MAX			1024
#define REG05_ITERM_OFFSET			64
#define REG05_ITERM_STEP			64
#define REG05_ITERM_MIN				64
#define REG05_ITERM_MAX				1024

#define REG06_VREG_OFFSET			3840
#define REG06_VREG_STEP				16
#define REG06_VREG_MIN				3840
#define REG06_VREG_MAX				4608
#define REG06_BATLOWV_2p8v			0
#define REG06_BATLOWV_3v			1
#define REG06_VRECHG_100MV			0
#define REG06_VRECHG_200MV			1

#define REG07_TERM_DISABLE			0
#define REG07_TERM_ENABLE			1
#define REG07_STAT_DIS_DISABLE			1
#define REG07_STAT_DIS_ENABLE			0
#define REG07_WDT_DISABLE			0
#define REG07_WDT_40S				1
#define REG07_WDT_80S				2
#define REG07_WDT_160S				3
#define REG07_CHG_TIMER_DISABLE			0
#define REG07_CHG_TIMER_ENABLE			1
#define REG07_CHG_TIMER_5HOURS			0
#define REG07_CHG_TIMER_8HOURS			1
#define REG07_CHG_TIMER_12HOURS			2
#define REG07_CHG_TIMER_20HOURS			3


#define REG08_COMP_R_OFFSET			0
#define REG08_COMP_R_STEP			20
#define REG08_COMP_R_MIN			0
#define REG08_COMP_R_MAX			140
#define REG08_VCLAMP_OFFSET			0
#define REG08_VCLAMP_STEP			32
#define REG08_VCLAMP_MIN			0
#define REG08_VCLAMP_MAX			224
#define REG08_TREG_60				0
#define REG08_TREG_80				1
#define REG08_TREG_100				2
#define REG08_TREG_120				3


#define REG09_TMR2X_EN_DISABLE			0
#define REG09_TMR2X_EN_ENABLE			1
#define REG09_BATFET_DIS_DISABLE		0
#define REG09_BATFET_DIS_ENABLE			1

#define REG09_BATFET_DLY_DISABLE		0
#define REG09_BATFET_DLY_ENABLE			1
#define REG09_BATFET_RST_EN_DISABLE		0
#define REG09_BATFET_RST_EN_ENABLE		1
#define REG09_PUMPX_UP_DISABLE			0
#define REG09_PUMPX_UP_ENABLE			1
#define REG09_PUMPX_DN_DISABLE			0
#define REG09_PUMPX_DN_ENABLE			1

#define REG0A_BOOSTV_OFFSET			4550
#define REG0A_BOOSTV_STEP			64
#define REG0A_BOOSTV_MIN			4550
#define REG0A_BOOSTV_MAX			5510
#define REG0A_BOOSTV_LIM_OFFSET			1200
#define REG0A_BOOSTV_LIM_STEP			400
#define REG0A_BOOSTV_LIM_MIN			1200
#define REG0A_BOOSTV_LIM_MAX			4000


#define REG0B_VBUS_TYPE_NONE			0
#define REG0B_VBUS_TYPE_USB_SDP			1
#define REG0B_VBUS_TYPE_ADAPTER			2
#define REG0B_VBUS_TYPE_OTG			7


#define REG0B_CHRG_STAT_IDLE			0
#define REG0B_CHRG_STAT_PRECHG			1
#define REG0B_CHRG_STAT_FASTCHG			2
#define REG0B_CHRG_STAT_CHGDONE			3
#define REG0B_POWER_NOT_GOOD			0
#define REG0B_POWER_GOOD			1
#define REG0B_NOT_IN_VSYS_STAT			0
#define REG0B_IN_VSYS_STAT			1

#define REG0C_FAULT_WDT				1
#define REG0C_FAULT_BOOST			1
#define REG0C_FAULT_CHRG_NORMAL			0
#define REG0C_FAULT_CHRG_INPUT			1
#define REG0C_FAULT_CHRG_THERMAL		2
#define REG0C_FAULT_CHRG_TIMER			3
#define REG0C_FAULT_BAT_OVP			1


#define REG0D_FORCE_VINDPM_DISABLE		0
#define REG0D_FORCE_VINDPM_ENABLE		1
#define REG0D_VINDPM_OFFSET			2600
#define REG0D_VINDPM_STEP			100
#define REG0D_VINDPM_MIN			3900
#define REG0D_VINDPM_MAX			15300

#define REG0E_THERM_STAT			1


#define REG11_VBUS_GD				1

#define REG13_VDPM_STAT				1
#define REG13_IDPM_STAT				1


#define REG14_REG_RESET				1


/* Other Realted Definition*/
#define SGM41510_FGU_NAME			"sc27xx-fgu"

#define BIT_DP_DM_BC_ENB			BIT(0)

#define SGM41510_WDT_VALID_MS			50

#define SGM41510_WDG_TIMER_MS			15000
#define SGM41510_OTG_VALID_MS			500
#define SGM41510_OTG_RETRY_TIMES			10

#define SGM41510_DISABLE_PIN_MASK		BIT(0)
#define SGM41510_DISABLE_PIN_MASK_2721		BIT(15)

#define SGM41510_FAST_CHG_VOL_MAX		10500000
#define SGM41510_NORMAL_CHG_VOL_MAX		6500000

#define SGM41510_WAKE_UP_MS			2000

struct sgm41510_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sgm41510_dump_reg;
	struct device_attribute attr_sgm41510_lookup_reg;
	struct device_attribute attr_sgm41510_sel_reg_id;
	struct device_attribute attr_sgm41510_reg_val;
	struct attribute *attrs[5];

	struct sgm41510_charger_info *info;
};

struct sgm41510_charge_current {
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

struct sgm41510_charger_info {
	struct i2c_client *client;
	struct device *dev;

	struct sgm41510_charge_current cur;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work dump_work;
	struct regmap *pmic;
    struct alarm wdg_timer;
    struct sgm41510_charger_sysfs *sysfs;
	struct gpio_desc *gpiod;
	struct usb_phy *usb_phy;
	struct power_supply *psy_usb;
    u32 charger_detect;
    u32 charger_pd;
    u32 charger_pd_mask;
    u32 actual_limit_current;
    int termination_cur;
    int vol_max_mv;
    int vbus_ovp_mv;
    int reg_id;
    int limit;
    bool charging;
    bool otg_enable;
    bool ovp_enable;
    struct notifier_block usb_notify;
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	struct zlog_client *zlog_client;
#endif
	bool suspended;
	int chg_status;
};

#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
static struct zlog_mod_info zlog_bc_dev = {
	.module_no = ZLOG_MODULE_CHG,
	.name = "buck_charger",
	.device_name = "buckcharger",
	.ic_name = "sgm41510",
	.module_name = "BoardChip",
	.fops = NULL,
};
#endif

struct sgm41510_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sgm41510_charger_reg_tab reg_tab[SGM41510_REG_NUM + 1] = {
	{0, SGM41510_REG_00, "Setting Input Limit Current reg"},
	{1, SGM41510_REG_01, "Reserved reg"},
	{2, SGM41510_REG_02, "Related Function Enable reg"},
	{3, SGM41510_REG_03, "Related Function Config reg"},
	{4, SGM41510_REG_04, "Setting Charge Limit Current reg"},
	{5, SGM41510_REG_05, "Setting Terminal Current reg"},
	{6, SGM41510_REG_06, "Setting Charge Limit Voltage reg"},
	{7, SGM41510_REG_07, "Related Function Config reg"},
	{8, SGM41510_REG_08, "IR Compensation Resistor Setting reg"},
	{9, SGM41510_REG_09, "Related Function Config reg"},
	{10, SGM41510_REG_0A, "Boost Mode Related Setting reg"},
	{11, SGM41510_REG_0B, "Status reg"},
	{12, SGM41510_REG_0C, "Fault reg"},
	{13, SGM41510_REG_0D, "Setting Vindpm reg"},
	{14, SGM41510_REG_0E, "Thermal status reg"},
	{15, SGM41510_REG_0F, "Reserved reg"},
	{16, SGM41510_REG_10, "Reserved reg"},
	{17, SGM41510_REG_11, "VBUS status reg"},
	{18, SGM41510_REG_12, "Reserved reg"},
	{19, SGM41510_REG_13, "IDPM status reg"},
	{20, SGM41510_REG_14, "Related Function Config reg"},
	{21, SGM41510_REG_15, "Related Function Config reg"},
	{22, 0, "null"},
};

static int sgm41510_charger_set_limit_current(struct sgm41510_charger_info *info,
					     u32 limit_cur);

static int sgm41510_read(struct sgm41510_charger_info *info, u8 reg, u8 *data)
{
	int ret = 0, retry_cnt = 3;

	if (info->suspended) {
		ZTE_CM_IC_ERROR("not iic read after system suspend!\n");
		return 0;
	}

	do {
		ret = i2c_smbus_read_byte_data(info->client, reg);
		if (ret < 0) {
			ZTE_CM_IC_ERROR("sgm41510_read failed, ret=%d, retry_cnt=%d\n", ret, retry_cnt);
			usleep_range(5000, 5500);
		}
	} while ((ret < 0) && (retry_cnt-- > 0));

	if (ret < 0) {
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
		zlog_client_record(info->zlog_client, "i2c read err:%d\n", ret);
		zlog_client_notify(info->zlog_client, ZLOG_CHG_I2C_R_ERROR_NO);
#endif
		*data = 0;
		return ret;
	} else {
		*data = ret;
	}

	return 0;
}

static int sgm41510_write(struct sgm41510_charger_info *info, u8 reg, u8 data)
{
	int ret = 0, retry_cnt = 3;

	if (info->suspended) {
		ZTE_CM_IC_ERROR("not iic write after system suspend!\n");
		return 0;
	}

	do {
		ret = i2c_smbus_write_byte_data(info->client, reg, data);
		if (ret < 0) {
			ZTE_CM_IC_ERROR("sgm41510_write failed, ret=%d, retry_cnt=%d\n", ret, retry_cnt);
			usleep_range(5000, 5500);
		}
	} while ((ret < 0) && (retry_cnt-- > 0));

	if (ret < 0) {
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
		zlog_client_record(info->zlog_client, "i2c write err:%d\n", ret);
		zlog_client_notify(info->zlog_client, ZLOG_CHG_I2C_W_ERROR_NO);
#endif
		return ret;
	}

	return 0;
}

static int sgm41510_update_bits(struct sgm41510_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret = 0;

	ret = sgm41510_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sgm41510_write(info, reg, v);
}

static void sgm41510_charger_dump_register(struct sgm41510_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 value[SGM41510_REG_NUM] = {0};
	char buffer[512];
	char temp = 0;

	memset(buffer, '\0', sizeof(buffer));
	for (i = 0; i < SGM41510_REG_NUM; i++) {
		ret = sgm41510_read(info, reg_tab[i].addr, &value[i]);
		if (ret == 0) {
			len = snprintf(buffer + idx, sizeof(buffer) - idx,
				       "[0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       value[i]);
			idx += len;
		}
	}
	ZTE_CM_IC_INFO("%s", buffer);

	memset(buffer, '\0', sizeof(buffer));
	len = sizeof(buffer);
	temp = (value[11] & GENMASK(4, 3)) >> 3;

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
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: TERM CHARGING, ");
		break;
	default:
		break;
	}

	if (value[12] & BIT(7)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "WT_FAULT: 1, ");
	}

	if (value[12] & BIT(6)) {
		snprintf(buffer + strlen(buffer), len- strlen(buffer), "BOOST_FAULT: 1, ");
	}

	temp = (value[12] & GENMASK(5, 4)) >> 4;

	switch (temp) {
	case 1:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VAC OVP or VBUS UVP: 1, ");
		break;
	case 2:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "THERMAL SHUTDOWN: 1, ");
		break;
	case 3:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "SAFETY TIMEOUT: 1, ");
		break;
	default:
		break;
	}

	if (value[12] & BIT(3)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "BATOVP: 1, ");
	}

	if (value[19] & BIT(7)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 1, ");
		info->chg_status |= BIT(SQC_CHG_STATUS_AICL);
	} else {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 0, ");
		info->chg_status &= ~ BIT(SQC_CHG_STATUS_AICL);
	}

	if (value[19] & BIT(6)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "IINDPM: 1, ");
	}

	ZTE_CM_IC_INFO("%s", buffer);
}


static int sgm41510_charger_set_recharging_vol(struct sgm41510_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol <= 100)
        reg_val = REG06_VRECHG_100MV;
    else
        reg_val = REG06_VRECHG_200MV;

    return sgm41510_update_bits(info, SGM41510_REG_06, REG06_VRECHG_MASK,
                               reg_val << REG06_VRECHG_SHIFT);
}

static int sgm41510_charger_get_recharging_vol(struct sgm41510_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sgm41510_read(info, SGM41510_REG_06, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read sc8989x termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG06_VRECHG_MASK) >> REG06_VRECHG_SHIFT;

    *vol = reg_val ? 200 : 100;

    return 0;
}

static int sgm41510_charger_set_hiz(struct sgm41510_charger_info *info, u32 en_hiz)
{
    u8 reg_val = 0;

    if (en_hiz)
        reg_val = REG00_EN_HIZ;
    else
        reg_val = REG00_EXIT_HIZ;

    return sgm41510_update_bits(info, SGM41510_REG_00, REG00_ENHIZ_MASK,
                               reg_val << REG00_ENHIZ_SHIFT);
}

static int sgm41510_charger_get_hiz(struct sgm41510_charger_info *info, u32 *en_hiz)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sgm41510_read(info, SGM41510_REG_00, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read sc8989x termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

    *en_hiz = reg_val ? REG00_EN_HIZ : REG00_EXIT_HIZ;

    return 0;
}

static int sgm41510_charger_set_vindpm(struct sgm41510_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret = 0;

	ret = sgm41510_update_bits(info, SGM41510_REG_0D, REG0D_FORCE_VINDPM_MASK,
				  REG0D_FORCE_VINDPM_ENABLE << REG0D_FORCE_VINDPM_SHIFT);
	if (ret) {
		dev_err(info->dev, "set force vindpm failed\n");
		return ret;
	}

	if (vol < REG0D_VINDPM_MIN)
		vol = REG0D_VINDPM_MIN;
	else if (vol > REG0D_VINDPM_MAX)
		vol = REG0D_VINDPM_MAX;
	reg_val = (vol - REG0D_VINDPM_OFFSET) / REG0D_VINDPM_STEP;

	return sgm41510_update_bits(info, SGM41510_REG_0D,
				   REG0D_VINDPM_MASK, reg_val);
}

static int sgm41510_charger_set_termina_vol(struct sgm41510_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG06_VREG_MIN)
		vol = REG06_VREG_MIN;
	else if (vol >= REG06_VREG_MAX)
		vol = REG06_VREG_MAX;
	reg_val = (vol - REG06_VREG_OFFSET) / REG06_VREG_STEP;

	return sgm41510_update_bits(info, SGM41510_REG_06, REG06_VREG_MASK,
				   reg_val << REG06_VREG_SHIFT);
}

static int sgm41510_charger_get_termina_vol(struct sgm41510_charger_info *info, u32 *termina_vol)
{
	u8 reg_val;
	int ret = 0;

	ret = sgm41510_read(info, SGM41510_REG_06, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG06_VREG_MASK;
	reg_val = reg_val >> REG06_VREG_SHIFT;

	*termina_vol = reg_val * REG06_VREG_STEP + REG06_VREG_OFFSET;
	if (*termina_vol >= REG06_VREG_MAX)
		*termina_vol = REG06_VREG_MAX;

	return 0;
}


static int sgm41510_charger_set_termina_cur(struct sgm41510_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= REG05_ITERM_MIN)
		cur = REG05_ITERM_MIN;
	else if (cur >= REG05_ITERM_MAX)
		cur = REG05_ITERM_MAX;
	reg_val = (cur - REG05_ITERM_OFFSET) / REG05_ITERM_STEP;

	return sgm41510_update_bits(info, SGM41510_REG_05, REG05_ITERM_MASK, reg_val);
}

static int sgm41510_charger_get_termina_cur(struct sgm41510_charger_info *info, u32 *termina_cur)
{
	u8 reg_val;
	int ret = 0;

	ret = sgm41510_read(info, SGM41510_REG_05, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG05_ITERM_MASK;
	*termina_cur = reg_val * REG05_ITERM_STEP + REG05_ITERM_OFFSET;
	if (*termina_cur >= REG05_ITERM_MAX)
		*termina_cur = REG05_ITERM_MAX;

	return 0;
}

static int sgm41510_charger_hw_init(struct sgm41510_charger_info *info)
{
	int ret = 0;

	ret = sgm41510_update_bits(info, SGM41510_REG_14, REG14_REG_RESET_MASK,
				  REG14_REG_RESET << REG14_REG_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm41510 failed\n");
		return ret;
	}

	ret = sgm41510_charger_set_vindpm(info, 4600);
	if (ret) {
		dev_err(info->dev, "set sgm41510 vindpm vol failed\n");
		return ret;
	}

	ret = sgm41510_charger_set_termina_vol(info, 4200);
	if (ret) {
		dev_err(info->dev, "set sgm41510 terminal vol failed\n");
		return ret;
	}

	ret = sgm41510_charger_set_termina_cur(info, 300);
	if (ret) {
		dev_err(info->dev, "set sgm41510 terminal cur failed\n");
		return ret;
	}

	ret = sgm41510_charger_set_limit_current(info, 1000);
	if (ret)
		dev_err(info->dev, "set sgm41510 limit current failed\n");

	ret = sgm41510_update_bits(info, SGM41510_REG_0A, REG0A_BOOSTV_LIM_MASK, 0); // OTG current limit 1.2A
	if (ret) {
		dev_err(info->dev, "set sgm41510 boost limit current failed\n");
		return ret;
	}

	ret = sgm41510_update_bits(info, SGM41510_REG_07, REG07_CHG_TIMER_MASK,
				  REG07_CHG_TIMER_20HOURS << REG07_CHG_TIMER_SHIFT);
	if (ret) {
		dev_err(info->dev, "set safty timer 20 hours failed\n");
		return ret;
	}

	return ret;
}

static int sgm41510_charger_start_charge(struct sgm41510_charger_info *info)
{
	int ret = 0;

	ret = sgm41510_update_bits(info, SGM41510_REG_00,
				  REG00_ENHIZ_MASK, REG00_HIZ_DISABLE);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sgm41510_update_bits(info, SGM41510_REG_07, REG07_WDT_MASK,
				  REG07_WDT_40S << REG07_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm41510 watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable sgm41510 charge failed\n");
			return ret;
		}

	ret = sgm41510_update_bits(info, SGM41510_REG_03, REG03_CHG_CONFIG_MASK,
				  REG03_CHG_ENABLE << REG03_CHG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm41510 CHG_CONFIG\n");
		return ret;
	}

	return ret;
}

static int sgm41510_charger_stop_charge(struct sgm41510_charger_info *info)
{
	int ret = 0;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable sgm41510 charge failed\n");

	ret = sgm41510_update_bits(info, SGM41510_REG_07, REG07_WDT_MASK,
				  REG07_WDT_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable sgm41510 watchdog\n");

	ret = sgm41510_update_bits(info, SGM41510_REG_03, REG03_CHG_CONFIG_MASK,
				  REG03_CHG_DISABLE << REG03_CHG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to disable sgm41510 CHG_CONFIG\n");
		return ret;
	}

	return ret;
}

static int sgm41510_charger_get_charge_status(struct sgm41510_charger_info *info, u32 *chg_enable)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sgm41510_read(info, SGM41510_REG_03, &reg_val);
    if (ret)
        ZTE_CM_IC_ERROR("Failed to read sc8989x charge_status\n");

    *chg_enable = (reg_val & REG03_CHG_CONFIG_MASK) ? 1 : 0;

    return ret;
}

static int sgm41510_charger_set_current(struct sgm41510_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret = 0;

	if (cur <= REG04_ICHG_MIN)
		cur = REG04_ICHG_MIN;
	else if (cur >= REG04_ICHG_MAX)
		cur = REG04_ICHG_MAX;
	reg_val = cur / REG04_ICHG_STEP;

	ret = sgm41510_update_bits(info, SGM41510_REG_04, REG04_ICHG_MASK, reg_val);
	dev_err(info->dev, "current = %d, reg_val = %x\n", cur, reg_val);
	return ret;
}

static int sgm41510_charger_get_current(struct sgm41510_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret = 0;

	ret = sgm41510_read(info, SGM41510_REG_04, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG04_ICHG_MASK;
	*cur = reg_val * REG04_ICHG_STEP;

	return 0;
}

static int sgm41510_charger_set_limit_current(struct sgm41510_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret = 0;

	mutex_lock(&info->input_limit_cur_lock);
/*
	if (enable) {
		ret = sgm41510_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}

		if (limit_cur == info->actual_limit_current)
			goto out;
		limit_cur = info->actual_limit_current;
	}
*/
	ret = sgm41510_update_bits(info, SGM41510_REG_00, REG00_EN_ILIM_MASK,
				  REG00_EN_ILIM_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable en_ilim failed\n");
		goto out;
	}

	if (limit_cur >= REG00_IINLIM_MAX)
		limit_cur = REG00_IINLIM_MAX;
	if (limit_cur <= REG00_IINLIM_MIN)
		limit_cur = REG00_IINLIM_MIN;

	reg_val = (limit_cur - REG00_IINLIM_OFFSET) / REG00_IINLIM_STEP;

	ret = sgm41510_update_bits(info, SGM41510_REG_00, REG00_IINLIM_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sgm41510 limit cur failed\n");

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}


static u32 sgm41510_charger_get_limit_current(struct sgm41510_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret = 0;

	ret = sgm41510_read(info, SGM41510_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	*limit_cur = reg_val * REG00_IINLIM_STEP + REG00_IINLIM_OFFSET;
	if (*limit_cur >= REG00_IINLIM_MAX)
		*limit_cur = REG00_IINLIM_MAX;

	return 0;
}

static int sgm41510_charger_feed_watchdog(struct sgm41510_charger_info *info)
{
	int ret = 0;

	ret = sgm41510_update_bits(info, SGM41510_REG_03, REG03_WDT_RESET_MASK,
				  REG03_WDT_RESET << REG03_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset sgm41510 failed\n");
		return ret;
	}

	return ret;
}

static ssize_t sgm41510_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_reg_val);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;
	u8 val;
	int ret = 0;

	if (!info)
		return sprintf(buf, "sgm41510_sysfs->info is null\n");

	ret = sgm41510_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get sgm41510_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get sgm41510_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "sgm41510_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t sgm41510_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_reg_val);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;
	u8 val;
	int ret = 0;

	if (!info) {
		dev_err(dev, "sgm41510_sysfs->info is null\n");
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sgm41510_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sgm41510_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_sel_reg_id);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "sgm41510_sysfs->info is null\n");
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sgm41510_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SGM41510_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sgm41510_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", sgm41510_sysfs->name, id);
	return count;
}

static ssize_t sgm41510_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_sel_reg_id);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm41510_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sgm41510_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_lookup_reg);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2000];

	if (!info)
		return sprintf(buf, "%s sgm41510_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < SGM41510_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sgm41510_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs =
		container_of(attr, struct sgm41510_charger_sysfs,
			     attr_sgm41510_dump_reg);
	struct sgm41510_charger_info *info = sgm41510_sysfs->info;

	if (!info)
		return sprintf(buf, "%s sgm41510_sysfs->info is null\n", __func__);

	sgm41510_charger_dump_register(info);

	return sprintf(buf, "%s\n", sgm41510_sysfs->name);
}

static int sgm41510_register_sysfs(struct sgm41510_charger_info *info)
{
	struct sgm41510_charger_sysfs *sgm41510_sysfs;
	int ret = 0;

	sgm41510_sysfs = devm_kzalloc(info->dev, sizeof(*sgm41510_sysfs), GFP_KERNEL);
	if (!sgm41510_sysfs)
		return -ENOMEM;

	info->sysfs = sgm41510_sysfs;
	sgm41510_sysfs->name = "sgm41510_sysfs";
	sgm41510_sysfs->info = info;
	sgm41510_sysfs->attrs[0] = &sgm41510_sysfs->attr_sgm41510_dump_reg.attr;
	sgm41510_sysfs->attrs[1] = &sgm41510_sysfs->attr_sgm41510_lookup_reg.attr;
	sgm41510_sysfs->attrs[2] = &sgm41510_sysfs->attr_sgm41510_sel_reg_id.attr;
	sgm41510_sysfs->attrs[3] = &sgm41510_sysfs->attr_sgm41510_reg_val.attr;
	sgm41510_sysfs->attrs[4] = NULL;
	sgm41510_sysfs->attr_g.name = "debug";
	sgm41510_sysfs->attr_g.attrs = sgm41510_sysfs->attrs;

	sysfs_attr_init(&sgm41510_sysfs->attr_sgm41510_dump_reg.attr);
	sgm41510_sysfs->attr_sgm41510_dump_reg.attr.name = "sgm41510_dump_reg";
	sgm41510_sysfs->attr_sgm41510_dump_reg.attr.mode = 0444;
	sgm41510_sysfs->attr_sgm41510_dump_reg.show = sgm41510_dump_reg_show;

	sysfs_attr_init(&sgm41510_sysfs->attr_sgm41510_lookup_reg.attr);
	sgm41510_sysfs->attr_sgm41510_lookup_reg.attr.name = "sgm41510_lookup_reg";
	sgm41510_sysfs->attr_sgm41510_lookup_reg.attr.mode = 0444;
	sgm41510_sysfs->attr_sgm41510_lookup_reg.show = sgm41510_reg_table_show;

	sysfs_attr_init(&sgm41510_sysfs->attr_sgm41510_sel_reg_id.attr);
	sgm41510_sysfs->attr_sgm41510_sel_reg_id.attr.name = "sgm41510_sel_reg_id";
	sgm41510_sysfs->attr_sgm41510_sel_reg_id.attr.mode = 0644;
	sgm41510_sysfs->attr_sgm41510_sel_reg_id.show = sgm41510_reg_id_show;
	sgm41510_sysfs->attr_sgm41510_sel_reg_id.store = sgm41510_reg_id_store;

	sysfs_attr_init(&sgm41510_sysfs->attr_sgm41510_reg_val.attr);
	sgm41510_sysfs->attr_sgm41510_reg_val.attr.name = "sgm41510_reg_val";
	sgm41510_sysfs->attr_sgm41510_reg_val.attr.mode = 0644;
	sgm41510_sysfs->attr_sgm41510_reg_val.show = sgm41510_reg_val_show;
	sgm41510_sysfs->attr_sgm41510_reg_val.store = sgm41510_reg_val_store;

	ret = sysfs_create_group(&info->dev->kobj, &sgm41510_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sgm41510_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{

	val->intval  = 0;
	
	return 0;
}

static int sgm41510_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	return 0;

}

static int sgm41510_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_CALIBRATE:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sgm41510_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc sgm41510_charger_desc = {
	.name			= "charger_psy",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sgm41510_usb_props,
	.num_properties		= ARRAY_SIZE(sgm41510_usb_props),
	.get_property		= sgm41510_charger_usb_get_property,
	.set_property		= sgm41510_charger_usb_set_property,
	.property_is_writeable	= sgm41510_charger_property_is_writeable,
};


static void sgm41510_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41510_charger_info *info = container_of(dwork,
							 struct sgm41510_charger_info,
							 wdt_work);
	int ret = 0;

	ret = sgm41510_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 5);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool sgm41510_charger_check_otg_valid(struct sgm41510_charger_info *info)
{
	int ret = 0;
	u8 value = 0;
	bool status = false;

	ret = sgm41510_read(info, SGM41510_REG_03, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41510 charger otg valid status failed\n");
		return status;
	}

	if (value & REG03_OTG_CONFIG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_3 = 0x%x\n", value);

	return status;
}

static bool sgm41510_charger_check_otg_fault(struct sgm41510_charger_info *info)
{
	int ret = 0;
	u8 value = 0;
	bool status = true;

	ret = sgm41510_read(info, SGM41510_REG_0C, &value);
	if (ret) {
		dev_err(info->dev, "get sgm41510 charger otg fault status failed\n");
		return status;
	}

	if (!(value & REG0C_FAULT_BOOST_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_0C = 0x%x\n",
			value);

	return status;
}

static void sgm41510_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41510_charger_info *info = container_of(dwork,
			struct sgm41510_charger_info, otg_work);
	bool otg_valid = sgm41510_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = sgm41510_charger_check_otg_fault(info);
		if (!otg_fault) {
			ret = sgm41510_update_bits(info, SGM41510_REG_03,
						  REG03_OTG_CONFIG_MASK,
						  REG03_OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);
			if (ret)
				dev_err(info->dev, "restart sgm41510 charger otg failed\n");
		}

		otg_valid = sgm41510_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SGM41510_OTG_RETRY_TIMES);

	if (retry >= SGM41510_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sgm41510_charger_enable_otg(struct regulator_dev *dev)
{
	struct sgm41510_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_ERROR("enable otg into\n");

	info->otg_enable = true;

	mutex_lock(&info->lock);

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		mutex_unlock(&info->lock);
		return ret;
	}

	ret = sgm41510_update_bits(info, SGM41510_REG_03, REG03_OTG_CONFIG_MASK,
				  REG03_OTG_ENABLE << REG03_OTG_CONFIG_SHIFT);

	if (ret) {
		dev_err(info->dev, "enable sgm41510 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		mutex_unlock(&info->lock);
		return ret;
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SGM41510_WDT_VALID_MS));
	schedule_delayed_work(&info->dump_work, msecs_to_jiffies(100));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SGM41510_OTG_VALID_MS));

	mutex_unlock(&info->lock);

	return 0;
}

static int sgm41510_charger_disable_otg(struct regulator_dev *dev)
{
	struct sgm41510_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_ERROR("enable otg into\n");

	info->otg_enable = false;

	mutex_lock(&info->lock);

	cancel_delayed_work_sync(&info->dump_work);
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sgm41510_update_bits(info, SGM41510_REG_03,
				  REG03_OTG_CONFIG_MASK, REG03_OTG_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable sgm41510 otg failed\n");
		mutex_unlock(&info->lock);
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
	if (ret)
		dev_err(info->dev, "enable BC1.2 failed\n");

	mutex_unlock(&info->lock);

	return ret;
}

static int sgm41510_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sgm41510_charger_info *info = rdev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_INFO("otg_is_enabled %d!\n", info->otg_enable);

	return info->otg_enable;
}

static const struct regulator_ops sgm41510_charger_vbus_ops = {
	.enable = sgm41510_charger_enable_otg,
	.disable = sgm41510_charger_disable_otg,
	.is_enabled = sgm41510_charger_vbus_is_enabled,
};

static const struct regulator_desc sgm41510_charger_vbus_desc = {
	.name = "charger_otg_vbus",
	.of_match = "charger_otg_vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sgm41510_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int sgm41510_charger_register_vbus_regulator(struct sgm41510_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &sgm41510_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int sgm41510_charger_register_vbus_regulator(struct sgm41510_charger_info *info)
{
	return 0;
}
#endif

static int sgm41510_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sgm41510_charger_info *info =
		container_of(nb, struct sgm41510_charger_info, usb_notify);

	ZTE_CM_IC_INFO("%d\n", limit);
	info->limit = limit;

#ifdef CONFIG_VENDOR_SQC_CHARGER
	sqc_notify_daemon_changed(SQC_NOTIFY_USB,
					SQC_NOTIFY_USB_STATUS_CHANGED, !!limit);
#endif

	return NOTIFY_OK;
}

static void sgm41510_charger_dump_reg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sgm41510_charger_info *info = container_of(dwork,
					struct sgm41510_charger_info, dump_work);

	sgm41510_charger_dump_register(info);

	schedule_delayed_work(&info->dump_work, HZ * 15);
}

#ifdef CONFIG_ZTE_POWER_SUPPLY_COMMON
#ifndef CONFIG_FAST_CHARGER_SC27XX
static int zte_sqc_set_prop_by_name(const char *name, enum zte_power_supply_property psp, int data)
{
	struct zte_power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		ZTE_CM_IC_INFO("sgm41510:psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = zte_power_supply_get_by_name(name);
	if (!psy) {
		ZTE_CM_IC_INFO("sgm41510:get %s psy failed!!\n", name);
		goto failed_loop;
	}

	val.intval = data;

	rc = zte_power_supply_set_property(psy,
				psp, &val);
	if (rc < 0) {
		ZTE_CM_IC_INFO("sgm41510:Failed to set %s property:%d rc=%d\n", name, psp, rc);
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
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;

	*charing_status = info->chg_status;

	return 0;
}

static int sqc_mp_set_enable_chging(void *arg, unsigned int en)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;

	if (en) {
#ifndef ZTE_FEATURE_PV_AR
		sgm41510_charger_feed_watchdog(info);
#endif
		sgm41510_charger_start_charge(info);

		sgm41510_charger_set_vindpm(info, 4600);

		info->charging = true;

		sgm41510_charger_dump_register(info);
	} else {
		sgm41510_charger_stop_charge(info);

		sgm41510_charger_dump_register(info);
		info->charging = false;
	}

	ZTE_CM_IC_INFO("%d\n", en);

	return 0;
}

static int sqc_mp_get_enable_chging(void *arg, unsigned int *en)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_charge_status(info, en);

	pr_info("sgm41510 %d\n", *en);

	return ret;
}

static int sqc_mp_get_ichg(void *arg, u32 *ichg_ma)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_current(info, ichg_ma);

	ZTE_CM_IC_INFO("sgm41510 %d\n", *ichg_ma);

	return ret;
}

static int sqc_mp_set_ichg(void *arg, u32 mA)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_set_current(info, mA);

	ZTE_CM_IC_INFO("sgm41510 %d\n", mA);

	return ret;
}

static int sqc_mp_set_ieoc(void *arg, u32 mA)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_set_termina_cur(info, mA);

	ZTE_CM_IC_INFO("sgm41510 %d\n", mA);

	return ret;
}

static int sqc_mp_get_ieoc(void *arg, u32 *ieoc)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_termina_cur(info, ieoc);

	ZTE_CM_IC_INFO("sgm41510 %d\n", *ieoc);

	return ret;
}

static int sqc_mp_get_aicr(void *arg, u32 *aicr_ma)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("sgm41510 %d\n", *aicr_ma);

	return ret;
}

static int sqc_mp_set_aicr(void *arg, u32 aicr_ma)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_set_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("sgm41510 %d\n", aicr_ma);

	return ret;
}

static int sqc_mp_get_cv(void *arg, u32 *cv_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("sgm41510 %d\n", *cv_mv);

	return ret;
}

static int sqc_mp_set_cv(void *arg, u32 cv_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_set_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("sgm41510 %d\n", cv_mv);

	return ret;
}

static int sqc_mp_get_rechg_voltage(void *arg, u32 *rechg_volt_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("sgm41510 %d\n", *rechg_volt_mv);

	return ret;
}

static int sqc_mp_set_rechg_voltage(void *arg, u32 rechg_volt_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_set_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("sgm41510 %d\n", rechg_volt_mv);

	return ret;
}

static int sqc_mp_ovp_volt_get(void *arg, u32 *ac_ovp_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;

    *ac_ovp_mv = info->vbus_ovp_mv;

    ZTE_CM_IC_INFO("sgm41510 %dmV\n", *ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_ovp_volt_set(void *arg, u32 ac_ovp_mv)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;

    info->vbus_ovp_mv = ac_ovp_mv;

    ZTE_CM_IC_INFO("sc8989x %dmV\n", ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_get_vbat(void *arg, u32 *mV)
{
	union power_supply_propval batt_vol_uv;
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SGM41510_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("%s: get failed!\n", SGM41510_FGU_NAME);
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

    ZTE_CM_IC_INFO("sgm41510 %dmV\n", *mV);

	return 0;
}

static int sqc_mp_get_ibat(void *arg, u32 *mA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge = NULL;
	int ret = 0;

	fuel_gauge = power_supply_get_by_name(SGM41510_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("%s: get failed!\n", SGM41510_FGU_NAME);
		return -ENODEV;
	}

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);

	power_supply_put(fuel_gauge);
	if (ret) {
		ZTE_CM_IC_ERROR("get POWER_SUPPLY_PROP_CURRENT_NOW failed!\n");
		return ret;
	}

	*mA = val.intval / 1000;

    ZTE_CM_IC_INFO("sgm41510 %mA\n", *mA);

    return 0;
}

static int sqc_mp_get_vbus(void *arg, u32 *mV)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)arg;
	union power_supply_propval val = {};
	struct power_supply *fuel_gauge = NULL;
	int ret = 0;

	fuel_gauge = power_supply_get_by_name(SGM41510_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("%s: get failed!\n", SGM41510_FGU_NAME);
		return -ENODEV;
	}

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);

	power_supply_put(fuel_gauge);
	if (ret) {
		ZTE_CM_IC_ERROR("get POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE failed!\n");
		return ret;
	}

	*mV = val.intval / 1000;

    ZTE_CM_IC_INFO("sgm41510 adc:%dmV, vbus_ovp_mv:%dmV\n", *mV, info->vbus_ovp_mv);

    if (*mV > info->vbus_ovp_mv) {
        if (info->ovp_enable != true) {
            info->ovp_enable = true;
            ZTE_CM_IC_INFO("sgm41510 vbus(%d) more that ovp(%d), enable hiz\n", *mV, info->vbus_ovp_mv);
            sgm41510_charger_set_hiz(info, true);
        }
    } else {
        if (info->ovp_enable != false) {
            info->ovp_enable = false;
            ZTE_CM_IC_INFO("sgm41510 vbus(%d) less that ovp(%d), disable hiz\n", *mV, info->vbus_ovp_mv);
            sgm41510_charger_set_hiz(info, false);
        }
    }

	return 0;
}

static int sqc_mp_get_ibus(void *arg, u32 *mA)
{
	arg = arg ? arg : NULL;

	*mA = 0;

	return 0;
}
/*
static int sqc_enable_powerpath_set(void *arg, unsigned int enabled)
{
	struct sgm41510_charger_info *chg_dev = (struct sgm41510_charger_info *)arg;

	return 0;
}

static int sqc_enable_powerpath_get(void *arg, unsigned int *enabled)
{
	struct sgm41510_charger_info *chg_dev = (struct sgm41510_charger_info *)arg;

	return 0;
}
*/
static int sqc_enable_hiz_set(void *arg, unsigned int enable)
{
	struct sgm41510_charger_info *chg_dev = (struct sgm41510_charger_info *)arg;

	sgm41510_charger_set_hiz(chg_dev, !!enable);

	return 0;
}

static int sqc_enable_hiz_get(void *arg, unsigned int *enable)
{
	struct sgm41510_charger_info *chg_dev = (struct sgm41510_charger_info *)arg;
	int ret = 0;

	ret = sgm41510_charger_get_hiz(chg_dev, enable);

	return ret;
}

static struct sqc_pmic_chg_ops sc8989x_sqc_chg_ops = {

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
/*
	.enable_path_set = sqc_enable_powerpath_set,
	.enable_path_get = sqc_enable_powerpath_get,
*/
	.enable_hiz_set = sqc_enable_hiz_set,
	.enable_hiz_get = sqc_enable_hiz_get,
};
#ifndef CONFIG_FAST_CHARGER_SC27XX
extern struct sqc_bc1d2_proto_ops sqc_bc1d2_proto_node;
static int sqc_chg_type = SQC_NONE_TYPE;

static int sqc_bc1d2_get_charger_type(unsigned int *chg_type)
{
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)sqc_bc1d2_proto_node.arg;

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
	struct sgm41510_charger_info *info = (struct sgm41510_charger_info *)sqc_bc1d2_proto_node.arg;

	sscanf(val, "%d", &sleep_mode_enable);

	ZTE_CM_IC_INFO("sleep_mode_enable = %d\n", sleep_mode_enable);

	if (sleep_mode_enable) {
		if (sqc_chg_type != SQC_SLEEP_MODE_TYPE) {
			ZTE_CM_IC_INFO("sleep on status");

			/*disabel sqc-daemon*/
			sqc_chg_type = SQC_SLEEP_MODE_TYPE;
			sqc_notify_daemon_changed(SQC_NOTIFY_USB, SQC_NOTIFY_USB_STATUS_CHANGED, 1);

            ret = sgm41510_update_bits(info, SGM41510_REG_07, REG07_WDT_MASK,
				  REG07_WDT_DISABLE);
			if (ret)
				dev_err(info->dev, "Failed to disable sgm41510 watchdog\n");

			/*mtk enter sleep mode*/
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

static int sgm41510_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sgm41510_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret = 0;
	struct sqc_pmic_chg_ops *sqc_ops = NULL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

    ZTE_CM_IC_INFO("initializing...\n");

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	alarm_init(&info->wdg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);

	INIT_DELAYED_WORK(&info->otg_work, sgm41510_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sgm41510_charger_feed_watchdog_work);
	INIT_DELAYED_WORK(&info->dump_work, sgm41510_charger_dump_reg_work);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		ZTE_CM_IC_ERROR("failed to find USB phy\n");
		goto err_mutex_lock;
	}

	ret = sgm41510_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SGM41510_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = SGM41510_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	info->usb_notify.notifier_call = sgm41510_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register notifier:%d\n", ret);
		goto err_mutex_lock;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &sgm41510_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_usb_notifier;
	}

	ret = sgm41510_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sgm41510_charger_hw_init\n");
		goto err_usb_notifier;
	}

	sgm41510_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);

	ret = sgm41510_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	ret = sgm41510_update_bits(info, SGM41510_REG_07, REG07_WDT_MASK,
				  REG07_WDT_40S << REG07_WDT_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to enable sgm41510 watchdog\n");
		return ret;
	}

	sqc_ops = kzalloc(sizeof(struct sqc_pmic_chg_ops), GFP_KERNEL);
    memcpy(sqc_ops, &sc8989x_sqc_chg_ops, sizeof(struct sqc_pmic_chg_ops));
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

#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
		info->zlog_client = zlog_register_client(&zlog_bc_dev);
		if (!info->zlog_client)
			ZTE_CM_IC_ERROR("zlog register client zlog_bc_dev fail\n");

		ZTE_CM_IC_INFO("zlog register client zlog_bc_dev success\n");
#endif
	info->suspended = false;

    ZTE_CM_IC_INFO("OK...\n");

	return 0;

err_sysfs:
	sysfs_remove_group(&info->dev->kobj, &info->sysfs->attr_g);
err_usb_notifier:
    usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_mutex_lock:
	mutex_destroy(&info->lock);

	return ret;
}

static void sgm41510_charger_shutdown(struct i2c_client *client)
{
	struct sgm41510_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->dump_work);
	if (info->otg_enable) {
		cancel_delayed_work_sync(&info->otg_work);
		ret = sgm41510_update_bits(info, SGM41510_REG_03,
					  REG03_OTG_CONFIG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sgm41510 otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int sgm41510_charger_remove(struct i2c_client *client)
{
	struct sgm41510_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	cancel_delayed_work_sync(&info->dump_work);
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sgm41510_charger_suspend(struct device *dev)
{
	struct sgm41510_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = SGM41510_WDG_TIMER_MS;

	if (info->otg_enable)
		/* feed watchdog first before suspend */
		sgm41510_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	info->suspended = true;

	cancel_delayed_work_sync(&info->dump_work);
	cancel_delayed_work_sync(&info->wdt_work);

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->wdg_timer, ktime_add(now, add));

	return 0;
}

static int sgm41510_charger_resume(struct device *dev)
{
	struct sgm41510_charger_info *info = dev_get_drvdata(dev);

	if (info->otg_enable)
		/* feed watchdog first before suspend */
		sgm41510_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->wdg_timer);

	info->suspended = false;

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	schedule_delayed_work(&info->dump_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops sgm41510_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sgm41510_charger_suspend,
				sgm41510_charger_resume)
};

static const struct i2c_device_id sgm41510_i2c_id[] = {
	{"sgm41510_chg", 0},
	{}
};

static const struct of_device_id sgm41510_charger_of_match[] = {
	{ .compatible = "sgm,sgm41510_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sgm41510_charger_of_match);

static struct i2c_driver sgm41510_charger_driver = {
	.driver = {
		.name = "sgm41510_chg",
		.of_match_table = sgm41510_charger_of_match,
		.pm = &sgm41510_charger_pm_ops,
	},
	.probe = sgm41510_charger_probe,
	.shutdown = sgm41510_charger_shutdown,
	.remove = sgm41510_charger_remove,
	.id_table = sgm41510_i2c_id,
};

module_i2c_driver(sgm41510_charger_driver);

MODULE_AUTHOR("Changhua Zhang <Changhua.Zhang@unisoc.com>");
MODULE_DESCRIPTION("SGM41510 Charger Driver");
MODULE_LICENSE("GPL v2");
