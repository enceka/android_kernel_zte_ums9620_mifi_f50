/*
 * Driver for the ETA Solutions hl7009 charger.
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

#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
#include <vendor/comdef/zlog_common_base.h>
#endif

int sqc_notify_daemon_changed(int chg_id, int msg_type, int msg_val);

#define CHARGER_IC_TAG   "[ZTE_LDD_CHARGE][CHARGER_IC][HL7009]"

#define ZTE_CM_IC_INFO(fmt, args...) {pr_info(CHARGER_IC_TAG"[Info]%s %d: "fmt, __func__, __LINE__, ##args); }

#define ZTE_CM_IC_ERROR(fmt, args...) {pr_err(CHARGER_IC_TAG"[Error]%s %d:  "fmt, __func__, __LINE__, ##args); }

#define ZTE_CM_IC_DEBUG(fmt, args...) {pr_debug(CHARGER_IC_TAG"[Debug]%s %d:  "fmt, __func__, __LINE__, ##args); }


#define HL7009_REG_0					0x0
#define HL7009_REG_1					0x1
#define HL7009_REG_2					0x2
#define HL7009_REG_3					0x3
#define HL7009_REG_4					0x4
#define HL7009_REG_5					0x5
#define HL7009_REG_6					0x6
#define HL7009_REG_7					0x7
#define HL7009_REG_8					0x8
#define HL7009_REG_9					0x9
#define HL7009_REG_A					0xa
#define HL7009_REG_B					0xb
#define HL7009_REG_C					0xc
/* Register bits */
/* HL7009_REG_0 (0x00) status and control reg*/
#define HL7009_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define HL7009_REG_EN_HIZ_SHIFT		(7)
#define HL7009_REG_EN_HIZ           (1)
#define HL7009_REG_EXIT_HIZ           (0)
#define HL7009_REG_VBUS_OVP_MASK			GENMASK(6, 5)
#define HL7009_REG_VBUS_OVP_SHIFT			(5)
#define HL7009_REG_IIN_DPM_MASK			GENMASK(4, 0)
#define HL7009_REG_IIN_DPM_SHIFT			(0)

/* HL7009_REG_1 (0x01) control reg */
#define HL7009_REG_CHG_EN_MASK		GENMASK(7, 7)
#define HL7009_REG_CHG_EN_SHIFT		(7)
#define HL7009_REG_WD_TMR_MASK		GENMASK(6, 5)
#define HL7009_REG_WD_TMR_SHIFT		(5)
#define HL7009_REG_WD_TMR_40S			1
#define HL7009_REG_WD_TMR_80S			2
#define HL7009_REG_WD_TMR_160S			3
#define HL7009_REG_WD_TMR_BASE			0
#define HL7009_REG_WD_TMR_LSB			40
#define HL7009_REG_WD_TMR_DISABLE		0

#define HL7009_REG_FSEL_MASK		GENMASK(4, 3)
#define HL7009_REG_FSEL_SHIFT		(3)

/* HL7009_REG_2 (0x02) control and battery voltage reg*/
#define HL7009_REG_ICHG_CC_MASK				GENMASK(5, 0)
#define HL7009_REG_ICHG_CC_SHIFT			(0)
#define HL7009_REG_TREG_MASK                GENMASK(6, 6)
#define HL7009_REG_TREG_SHIFT               (6)
#define HL7009_TREG_110C                (1)
#define HL7009_TREG_90C                 (0)



/* HL7009_REG_3 (0x03) Vendor/Part/Revision reg */
#define HL7009_REG_VENDOR_ID_MASK			GENMASK(7, 5)
#define HL7009_REG_VENDOR_ID_SHIFT			(5)
#define HL7009_REG_DEVICE_ID_MASK			GENMASK(4, 3)
#define HL7009_REG_DEVICE_ID_SHIFT			(3)

/* HL7009_REG_4 (0x04) battery termination and fast charge current reg*/
#define HL7009_REG_EN_TERM_MASK				GENMASK(7, 7)
#define HL7009_REG_EN_TERM_SHIFT				(7)
#define HL7009_REG_ICHG_TERM_MASK			GENMASK(6, 3)
#define HL7009_REG_ICHG_TERM_SHIFT				(3)
#define HL7009_REG_ITERM_DEG_MASK				GENMASK(2, 1)
#define HL7009_REG_ITERM_DEG_SHIFT				(1)

#define HL7009_REG_ICHG_TERM_LSB			60
#define HL7009_REG_ICHG_TERM_BASE 			120
#define HL7009_REG_ICHG_TERM_MIN			120
#define HL7009_REG_ICHG_TERM_MAX 			1440


/* HL7009_REG_5 (0x05) special charger voltage and enable pin status reg*/
#define HL7009_REG_VBAT_REG_MASK                  GENMASK(7, 1)
#define HL7009_REG_VBAT_REG_SHIFT                 (1)

#define HL7009_REG_VBAT_REG_MIN     (3900)
#define HL7009_REG_VBAT_REG_MAX     (4700)
#define HL7009_REG_VREG_BASE        (3900)
#define HL7009_REG_VREG_LSB        (10)

/* HL7009_REG_6 (0x06) savety limit reg*/
#define HL7009_REG_VINDPM_MASK				GENMASK(7, 2)
#define HL7009_REG_VINDPM_SHIFT			(2)
#define HL7009_REG_VBAT_RECHG_MASK				GENMASK(1, 0)
#define HL7009_REG_VBAT_RECHG_SHIFT				(0)

#define HL7009_REG_VRECHG_100MV          (0)
#define HL7009_REG_VRECHG_200MV          (1)
#define HL7009_REG_VRECHG_300MV          (2)
#define HL7009_REG_VRECHG_400MV          (3)

/* HL7009_REG_7 (0x07) OTG enable and BST_LIMIT Setting reg */
#define HL7009_REG_OTG_EN_MASK				GENMASK(7, 7)
#define HL7009_REG_OTG_EN_SHIFT			(7)
#define HL7009_REG_BST_LIMIT_MASK				GENMASK(4, 3)
#define HL7009_REG_BST_LIMIT_SHIFT			(3)

/* HL7009_REG_9 (0x09) Vbus and chg status reg,only read*/
#define HL7009_REG_VBUS_STAT_MASK                     GENMASK(7, 4)
#define HL7009_REG_VBUS_STAT_SHIFT			(4)
#define HL7009_REG_CHG_STAT_MASK                     GENMASK(3, 1)
#define HL7009_REG_CHG_STAT_SHIFT			(1)
#define HL7009_REG_VBUS_GD_STAT_MASK                     GENMASK(0, 0)
#define HL7009_REG_VBUS_GD_STAT_SHIFT			(0)

/* HL7009_REG_B (0x0b) REG_RST reset*/
#define HL7009_REG_RST_MASK				GENMASK(7, 7)
#define HL7009_REG_RST_SHIFT			(7)

/* HL7009_REG_C (0x0c) WD_RST reset*/
#define HL7009_REG_WD_RST_MASK				GENMASK(7, 7)
#define HL7009_REG_WD_RST_SHIFT			(7)
#define HL7009_REG_BST_FLT_FLG_MASK         GENMASK(2, 0)

#define HL7009_FGU_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define HL7009_OTG_VALID_MS				(500)
#define HL7009_FEED_WATCHDOG_VALID_MS			(50)
#define HL7009_WDG_TIMER_S				(15)

#define HL7009_OTG_TIMER_FAULT				(0x6)

#define HL7009_DISABLE_PIN_MASK_2730			BIT(0)
#define HL7009_DISABLE_PIN_MASK_2721			BIT(15)
#define HL7009_DISABLE_PIN_MASK_2720			BIT(0)

#define HL7009_CHG_IMMIN		(550000)
#define HL7009_CHG_IMSTEP		(200000)
#define HL7009_CHG_IMMAX		(3050000)
#define HL7009_CHG_IMIN		(550000)
#define HL7009_CHG_ISTEP		(100000)
#define HL7009_CHG_IMAX		(3050000)
#define HL7009_CHG_IMAX30500		(0x19)

#define HL7009_CHG_VMREG_MIN		(3900)
#define HL7009_CHG_VMREG_STEP		(10)
#define HL7009_CHG_VMREG_MAX		(4700)
#define HL7009_CHG_VOREG_MIN		(3500)
#define HL7009_CHG_VOREG_STEP		(20)
#define HL7009_CHG_VOREG_MAX		(4440)

#define HL7009_REG_VOREG		(BIT(5) | BIT(3))
#define HL7009_IIN_LIM_SEL		(1)
#define HL7009_REG_ICHG_OFFSET		(0)
#define HL7009_REG_IIN_LIMIT1_MAX	(3000000)

#define ENABLE_CHARGE 0
#define DISABLE_CHARGE 1

#define REG_RESET (1)
#define CHG_VINDPM_MAX (8200)
#define CHG_VINDPM_MIN (3900)
#define CHG_VINDPM_BASE (3900)
#define CHG_VINDPM_LSB (100)

#define ITERM_MIN (120)
#define ITERM_MAX (1440)
#define ITERM_LSB (60)
#define ITERM_BASE (120)
#define ITERM_MASK	GENMASK(6, 3)

#define HL7009_FCHG_OVP_6V5 6500
#define HL7009_FCHG_OVP_9V 9000
#define HL7009_FCHG_OVP_10V8 10800
#define HL7009_FCHG_OVP_6V	6000

#define HL7009_REG_NUM 13

struct hl7009_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
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
	bool disable_power_path;
	struct usb_phy *usb_phy;
	int limit;
	bool ovp_enable;
	int vbus_ovp_mv;
	struct notifier_block usb_notify;
	bool enable_powerptah;
	int chg_status;
	u32 term_vol;
	struct power_supply_charge_current cur;
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	struct zlog_client *zlog_client;
#endif
	bool suspended;
};

#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
static struct zlog_mod_info zlog_bc_dev = {
	.module_no = ZLOG_MODULE_CHG,
	.name = "buck_charger",
	.device_name = "buckcharger",
	.ic_name = "hl7009",
	.module_name = "BoardChip",
	.fops = NULL,
};
#endif

struct hl7009_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct hl7009_charger_reg_tab reg_tab[HL7009_REG_NUM + 1] = {
    {0, HL7009_REG_0, "Setting Input Limit Current reg"},
    {1, HL7009_REG_1, "Setting Vindpm_OS reg"},
    {2, HL7009_REG_2, "Related Function Enable reg"},
    {3, HL7009_REG_3, "Related Function Config reg"},
    {4, HL7009_REG_4, "Setting Charge Limit Current reg"},
    {5, HL7009_REG_5, "Setting Terminal Current reg"},
    {6, HL7009_REG_6, "Setting Charge Limit Voltage reg"},
    {7, HL7009_REG_7, "Related Function Config reg"},
    {8, HL7009_REG_8, "IR Compensation Resistor Setting reg"},
    {9, HL7009_REG_9, "Related Function Config reg"},
    {10, HL7009_REG_A, "Boost Mode Related Setting reg"},
    {11, HL7009_REG_B, "Status reg"},
    {12, HL7009_REG_C, "Fault reg"},
    {13, 0, "null"},
};

/*add*/
static void power_path_control(struct hl7009_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;
	char *match;
	char result[5];

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		info->disable_power_path = false;
		return;
	}

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		info->disable_power_path = true;

	match = strstr(cmd_line, "androidboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("androidboot.mode=")),
			sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;
	}
}
/*add end*/

static int hl7009_charger_set_limit_current(struct hl7009_charger_info *info, u32 limit_cur);

static int hl7009_read(struct hl7009_charger_info *info, u8 reg, u8 *data)
{
	int ret = 0, retry_cnt = 3;

	if (info->suspended) {
		ZTE_CM_IC_ERROR("not iic read after system suspend!\n");
		return 0;
	}

	do {
		ret = i2c_smbus_read_byte_data(info->client, reg);
		if (ret < 0) {
			ZTE_CM_IC_ERROR("hl7009_read failed, ret=%d, retry_cnt=%d\n", ret, retry_cnt);
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

static int hl7009_write(struct hl7009_charger_info *info, u8 reg, u8 data)
{
	int ret = 0, retry_cnt = 3;

	if (info->suspended) {
		ZTE_CM_IC_ERROR("not iic write after system suspend!\n");
		return 0;
	}

	do {
		ret = i2c_smbus_write_byte_data(info->client, reg, data);
		if (ret < 0) {
			ZTE_CM_IC_ERROR("hl7009_write failed, ret=%d, retry_cnt=%d\n", ret, retry_cnt);
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

static int hl7009_update_bits(struct hl7009_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = hl7009_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return hl7009_write(info, reg, v);
}

static int hl7009_charger_set_vindpm(struct hl7009_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol < CHG_VINDPM_MIN)
        vol = CHG_VINDPM_MIN;
    else if (vol > CHG_VINDPM_MAX)
        vol = CHG_VINDPM_MAX;
    reg_val = (vol - CHG_VINDPM_BASE) / CHG_VINDPM_LSB;

    return hl7009_update_bits(info, HL7009_REG_6,
                               HL7009_REG_VINDPM_MASK, (reg_val << HL7009_REG_VINDPM_SHIFT));
}

static int hl7009_charger_set_recharging_vol(struct hl7009_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol <= 100)
        reg_val = HL7009_REG_VRECHG_100MV;
    else if (vol <= 200)
        reg_val = HL7009_REG_VRECHG_200MV;
    else if (vol <= 300)
        reg_val = HL7009_REG_VRECHG_300MV;
    else
        reg_val = HL7009_REG_VRECHG_400MV;

    return hl7009_update_bits(info, HL7009_REG_6, HL7009_REG_VBAT_RECHG_MASK,
                               reg_val << HL7009_REG_VBAT_RECHG_SHIFT);
}

static int hl7009_charger_get_recharging_vol(struct hl7009_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = hl7009_read(info, HL7009_REG_6, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read recharge_vol\n");
        return ret;
    }

    reg_val = (reg_val & HL7009_REG_VBAT_RECHG_MASK) >> HL7009_REG_VBAT_RECHG_SHIFT;

    switch(reg_val) {
        case 0:
            *vol = 100 * 1000;
            break;
        case 1:
            *vol = 200 * 1000;
            break;
        case 2:
            *vol = 300 * 1000;
            break;
        case 3:
            *vol = 400 * 1000;
            break;
        default:
		    ZTE_CM_IC_ERROR("Read recharge_vol failed,return 100mV\n");
            *vol = 100 * 1000;
	}

    return 0;
}

static int hl7009_charger_set_hiz(struct hl7009_charger_info *info, u32 en_hiz)
{
    u8 reg_val = 0;

    if (en_hiz)
        reg_val = HL7009_REG_EN_HIZ;
    else
        reg_val = HL7009_REG_EXIT_HIZ;


    return hl7009_update_bits(info, HL7009_REG_0, HL7009_REG_EN_HIZ_MASK,
                               reg_val << HL7009_REG_EN_HIZ_SHIFT);
}

static int hl7009_charger_get_hiz(struct hl7009_charger_info *info, u32 *en_hiz)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = hl7009_read(info, HL7009_REG_0, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & HL7009_REG_EN_HIZ_MASK) >> HL7009_REG_EN_HIZ_SHIFT;

    *en_hiz = reg_val ? HL7009_REG_EN_HIZ : HL7009_REG_EXIT_HIZ;

    return 0;
}

static int hl7009_charger_set_termina_vol(struct hl7009_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol < HL7009_REG_VBAT_REG_MIN)
        vol = HL7009_REG_VBAT_REG_MIN;
    else if (vol >= HL7009_REG_VBAT_REG_MAX)
        vol = HL7009_REG_VBAT_REG_MAX;
    reg_val = (vol - HL7009_REG_VREG_BASE) / HL7009_REG_VREG_LSB;

    return hl7009_update_bits(info, HL7009_REG_5, HL7009_REG_VBAT_REG_MASK,
                               reg_val << HL7009_REG_VBAT_REG_SHIFT);
}

static int hl7009_charger_get_termina_vol(struct hl7009_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = hl7009_read(info, HL7009_REG_5, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & HL7009_REG_VBAT_REG_MASK) >> HL7009_REG_VBAT_REG_SHIFT;

    *vol = (reg_val * HL7009_REG_VREG_LSB + HL7009_REG_VREG_BASE) * 1000;

    return 0;
}

static int hl7009_charger_set_termina_cur(struct hl7009_charger_info *info, u32 cur)
{
    u8 reg_val = 0;

    if (cur <= HL7009_REG_ICHG_TERM_MIN)
        cur = HL7009_REG_ICHG_TERM_MIN;
    else if (cur >= HL7009_REG_ICHG_TERM_MAX)
        cur = HL7009_REG_ICHG_TERM_MAX;
    reg_val = (cur - HL7009_REG_ICHG_TERM_BASE) / HL7009_REG_ICHG_TERM_LSB;

    return hl7009_update_bits(info, HL7009_REG_4, HL7009_REG_ICHG_TERM_MASK, (reg_val << HL7009_REG_ICHG_TERM_SHIFT));
}

static int hl7009_charger_get_termina_cur(struct hl7009_charger_info *info, u32 *cur)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = hl7009_read(info, HL7009_REG_4, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & HL7009_REG_ICHG_TERM_MASK) >> HL7009_REG_ICHG_TERM_SHIFT;

    *cur = (reg_val * HL7009_REG_ICHG_TERM_LSB + HL7009_REG_ICHG_TERM_BASE) * 1000;

    return 0;
}

static int hl7009_charger_set_treg(struct hl7009_charger_info *info, u32 temp)
{
	u8 reg_val;

	if (temp == 1)
		reg_val = 0x01;
	else
		reg_val = 0x00;

	return hl7009_update_bits(info, HL7009_REG_2, HL7009_REG_TREG_MASK,
				   (reg_val << HL7009_REG_TREG_SHIFT));
}

static int hl7009_charger_set_ovp(struct hl7009_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol <= 6500)
		reg_val = 0x00;
	else if (vol > 6500 && vol <= 9000)
		reg_val = 0x01;
	else
		reg_val = 0x02;

	ZTE_CM_IC_INFO(" vol:%d  reg_val: 0x%x\n", vol, reg_val);

	return hl7009_update_bits(info, HL7009_REG_0, HL7009_REG_VBUS_OVP_MASK,
				   (reg_val << HL7009_REG_VBUS_OVP_SHIFT ));
}

static int hl7009_charger_set_watchdog_timer(struct hl7009_charger_info *info,
					u32 timer)
{
	u8 reg_val = 0;

	if (timer >= 160)
		reg_val = HL7009_REG_WD_TMR_160S;
	else if (timer <= 0)
		reg_val = HL7009_REG_WD_TMR_DISABLE;
	else
		reg_val = (timer - HL7009_REG_WD_TMR_BASE) / HL7009_REG_WD_TMR_LSB;

	return hl7009_update_bits(info, HL7009_REG_1, HL7009_REG_WD_TMR_MASK,
					reg_val << HL7009_REG_WD_TMR_SHIFT);
}

static int hl7009_charger_start_charge(struct hl7009_charger_info *info)
{
	int ret = 0;

	ZTE_CM_IC_INFO("enter!\n");

	if (!IS_ERR(info->gpiod)) {
		gpiod_set_value_cansleep(info->gpiod, ENABLE_CHARGE);
	} else {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret)
			ZTE_CM_IC_ERROR("enable hl7009 charge failed ret = %d\n", ret);
	}

    ret = hl7009_update_bits(info, HL7009_REG_0,
                              HL7009_REG_EN_HIZ_MASK, HL7009_REG_EXIT_HIZ << HL7009_REG_EN_HIZ_SHIFT);
    if (ret)
        ZTE_CM_IC_ERROR("disable HIZ mode failed\n");

    ret = hl7009_update_bits(info, HL7009_REG_1, HL7009_REG_CHG_EN_MASK,
                              1 << HL7009_REG_CHG_EN_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to enable watchdog\n");
        return ret;
    }

	return ret;
}

static void hl7009_charger_stop_charge(struct hl7009_charger_info *info)
{
	int ret;

	ZTE_CM_IC_INFO("enter!\n");

	if (!IS_ERR(info->gpiod)) {
		ZTE_CM_IC_INFO("use gpiod\n");
		gpiod_set_value_cansleep(info->gpiod, DISABLE_CHARGE);
	} else {
		ZTE_CM_IC_INFO("use pmic pd\n");
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			ZTE_CM_IC_ERROR("disable hl7009 charge failed ret = %d\n", ret);
	}

	if (info->disable_power_path) {
		ret = hl7009_update_bits(info, HL7009_REG_0,
					  HL7009_REG_EN_HIZ_MASK,
					  0x00 << HL7009_REG_EN_HIZ_SHIFT);
		if (ret)
			ZTE_CM_IC_ERROR("Failed to disable power path\n");
	}
}

static int hl7009_charger_set_current(struct hl7009_charger_info *info, u32 cur)
{
	u8 reg_val ;

	/*if ICHG_OFFSET = 1, chg cur + 100mA*/
	if (cur >=3600) {
		reg_val = 0x3c;
	} else {
		reg_val = cur / 60;
	}

	reg_val = reg_val | 0x40;

	ZTE_CM_IC_INFO(" cur= %d, reg_val= 0x%02x\n", cur, reg_val);

	return hl7009_update_bits(info, HL7009_REG_2, HL7009_REG_ICHG_CC_MASK,
				    reg_val << HL7009_REG_ICHG_CC_SHIFT);
}

static int hl7009_charger_get_current(struct hl7009_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret = 0;

	ret = hl7009_read(info, HL7009_REG_2, &reg_val);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("set hl7009 ichg[2:0] cur failed ret = %d\n", ret);
		return ret;
	}
	reg_val = reg_val & 0x3f;
	if(reg_val >=0x3c)
	{
		*cur = 3600 * 1000;
	}else
	{
		*cur = 60 * reg_val * 1000;
	}
	return 0;
}


static int hl7009_charger_get_charge_status(struct hl7009_charger_info *info, u32 *chg_enable)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = hl7009_read(info, HL7009_REG_2, &reg_val);
    if (ret)
        ZTE_CM_IC_ERROR("Failed to read charge_status\n");

    *chg_enable = (reg_val & HL7009_REG_CHG_EN_MASK) ? 1 : 0;

    return ret;
}

static int hl7009_charger_set_input_limit_current(struct hl7009_charger_info *info, u32 limit_cur)
{
	u8 reg_val = 0;

	if(limit_cur >= 2500) {
		reg_val = 0x18;
	} else {
		reg_val = limit_cur / 100 - 1;
	}

	ZTE_CM_IC_INFO("limit_cur = %d, reg_val = 0x%02x\n", limit_cur, reg_val);

	return hl7009_update_bits(info, HL7009_REG_0, HL7009_REG_IIN_DPM_MASK,
				    (reg_val << HL7009_REG_IIN_DPM_SHIFT));
}

static int hl7009_charger_set_limit_current(struct hl7009_charger_info *info, u32 limit_cur)
{
	int ret;

	ret = hl7009_charger_set_input_limit_current(info, limit_cur);


	if (ret)
		ZTE_CM_IC_ERROR("set hl7009 ichg[2:0] cur failed ret = %d\n", ret);

	return ret;
}

static int hl7009_charger_get_input_limit_current(struct hl7009_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = hl7009_read(info, HL7009_REG_0, &reg_val);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("set hl7009 limit2 cur failed ret = %d\n", ret);
		return ret;
	}

	reg_val &= HL7009_REG_IIN_DPM_MASK;
	*limit_cur = 100 * (reg_val + 1 ) * 1000;

	ZTE_CM_IC_INFO(" reg_val = 0x%02x, limit_cur = %d\n", reg_val, *limit_cur);

	return ret;
}

static int hl7009_charger_get_limit_current(struct hl7009_charger_info *info, u32 *limit_cur)
{
	int ret;

	ret = hl7009_charger_get_input_limit_current(info, limit_cur);

	if (ret)
		ZTE_CM_IC_ERROR("set hl7009  get limit cur failed ret = %d\n", ret);

	return ret;
}

static int hl7009_charger_feed_watchdog(struct hl7009_charger_info *info)
{
	int ret;

	ret = hl7009_update_bits(info, 0x0c, HL7009_REG_WD_RST_MASK,
				  HL7009_REG_WD_RST_MASK);
	if (ret)
		ZTE_CM_IC_ERROR("hl7009 feed 0x0c watchdog failed ret = %d\n", ret);

	ZTE_CM_IC_INFO("done.\n");

	return ret;
}

static int hl7009_chip_name_show(struct seq_file *m, void *v)
{
	struct hl7009_charger_info *pinfo = m->private;

	if (pinfo) {
		seq_printf(m, "hl7009\n");
	} else {
		seq_printf(m, "unknow\n");
	}
	return 0;
}

static int hl7009_chip_name_open(struct inode *inode, struct file *file)
{
	return single_open(file, hl7009_chip_name_show, PDE_DATA(inode));
}

static const struct file_operations hl7009_chip_name_node = {
	.owner = THIS_MODULE,
	.open = hl7009_chip_name_open,
	.read = seq_read,
	.llseek = seq_lseek,
};


/*
static int hl7009_charger_get_status(struct hl7009_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int hl7009_charger_set_status(struct hl7009_charger_info *info, int val)
{
	int ret = 0;

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		hl7009_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = hl7009_charger_start_charge(info);
		if (ret)
			ZTE_CM_IC_ERROR("start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}
*/
static int hl7009_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	val->intval = 0;

	return 0;
}

static int hl7009_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	return 0;
}

static int hl7009_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property hl7009_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc hl7009_charger_desc = {
	.name			= "charger_psy",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= hl7009_usb_props,
	.num_properties		= ARRAY_SIZE(hl7009_usb_props),
	.get_property		= hl7009_charger_usb_get_property,
	.set_property		= hl7009_charger_usb_set_property,
	.property_is_writeable	= hl7009_charger_property_is_writeable,
};

static void hl7009_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7009_charger_info *info = container_of(dwork,
							 struct hl7009_charger_info,
							 wdt_work);
	int ret = 0;

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return;
	}

	ret = hl7009_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 5);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static void hl7009_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7009_charger_info *info = container_of(dwork,
			struct hl7009_charger_info, otg_work);
			
	int ret;
	u8 reg_val_0b, reg_val_07;

	ZTE_CM_IC_INFO("enter\n");

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
/* read otg stat*/
		ret = hl7009_read(info, HL7009_REG_B, &reg_val_0b);
		if(ret) {
			ZTE_CM_IC_ERROR("read_reg_val HL7009_REG_B fail !\n");
		} else {
			if(reg_val_0b &0x03 ) {
				/*set disable HIZ*/
				ZTE_CM_IC_INFO(" HL7009_REG_B = 0x%02x!\n", reg_val_0b);
				ret = hl7009_update_bits(info, HL7009_REG_0,
							HL7009_REG_EN_HIZ_MASK,
							0);
				if (ret) {
					ZTE_CM_IC_ERROR("failed to disable charger. ret = %d\n", ret);
					
				} else {
					/*enable otg*/
					ret = hl7009_update_bits(info, HL7009_REG_7,
								HL7009_REG_OTG_EN_MASK,
								HL7009_REG_OTG_EN_MASK);
					if (ret) {
						ZTE_CM_IC_ERROR("enable otg failed ret = %d\n", ret);
					}
				}
			} else {
				ret = hl7009_read(info, HL7009_REG_7, &reg_val_07);
				if(reg_val_07 & 0x80) {
					ZTE_CM_IC_INFO("OTG is good HL7009_REG_B = 0x%02x!\n", reg_val_0b);
				} else {
					/*enable otg*/
					ret = hl7009_update_bits(info, HL7009_REG_7,
								HL7009_REG_OTG_EN_MASK,
								HL7009_REG_OTG_EN_MASK);
					if (ret) {
						ZTE_CM_IC_ERROR("enable otg failed ret = %d\n", ret);
					}
					ZTE_CM_IC_INFO("OTG enable HL7009_REG_7 = 0x%02x!\n",reg_val_07);
				}
				
			}
		}
	}
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));			
}

static int hl7009_charger_enable_otg(struct regulator_dev *dev)
{
	struct hl7009_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;
	u8 reg_val = 0;

	ZTE_CM_IC_INFO("enter!\n");

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	info->otg_enable = true;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret)
		ZTE_CM_IC_ERROR("set charger_pd failed ret = %d\n", ret);

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
	ret = hl7009_update_bits(info, HL7009_REG_1,
				   HL7009_REG_CHG_EN_MASK,
				   0);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to disable charger. ret = %d\n", ret);
		return ret;
	}
	/* set EN_HIZ */
	ret = hl7009_update_bits(info, HL7009_REG_0,
				   HL7009_REG_EN_HIZ_MASK,
				   HL7009_REG_EN_HIZ_MASK);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to set hiz 1. ret = %d\n", ret);
		return ret;
	}
	ret = hl7009_update_bits(info, HL7009_REG_0,
				   HL7009_REG_EN_HIZ_MASK,
				   0);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to set hiz 0. ret = %d\n", ret);
		return ret;
	}
	/*enable otg*/
	ret = hl7009_update_bits(info, HL7009_REG_7,
				   HL7009_REG_OTG_EN_MASK,
				   HL7009_REG_OTG_EN_MASK);
	if (ret) {
		ZTE_CM_IC_ERROR("enable hl7009 otg failed ret = %d\n", ret);
		return ret;
	}
	
	/* read val*/
	if (hl7009_read(info, HL7009_REG_7, &reg_val)) {
		ZTE_CM_IC_ERROR("failed to get hl7009 otg status\n");
	} else {
		ZTE_CM_IC_INFO("reg:0x07 val:0x%x\n", reg_val);
	}
	
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(HL7009_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(HL7009_OTG_VALID_MS));

	ZTE_CM_IC_INFO("exit!\n");

	return 0;
}

static int hl7009_charger_disable_otg(struct regulator_dev *dev)
{
	struct hl7009_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	ZTE_CM_IC_INFO("enter!\n");

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	info->otg_enable = false;
	info->need_reinit = true;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	/*disable otg*/
	ret = hl7009_update_bits(info, HL7009_REG_7,
				   HL7009_REG_OTG_EN_MASK,
				   0);
	if (ret) {
		ZTE_CM_IC_ERROR("disable hl7009 otg failed ret = %d\n", ret);
		return ret;
	}
	/*enable charger*/
	ret = hl7009_update_bits(info, HL7009_REG_1,
				   HL7009_REG_CHG_EN_MASK,
				   HL7009_REG_CHG_EN_MASK);
	if (ret) {
		ZTE_CM_IC_ERROR("enable hl7009 charger failed ret = %d\n", ret);
		return ret;
	}
	ZTE_CM_IC_INFO("exit!\n");
	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int hl7009_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct hl7009_charger_info *info = rdev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR(" NULL pointer!!!\n");
		return -EINVAL;
	}

	ZTE_CM_IC_INFO("otg_is_enabled %d!\n", info->otg_enable);

	return info->otg_enable;
}

static const struct regulator_ops hl7009_charger_vbus_ops = {
	.enable = hl7009_charger_enable_otg,
	.disable = hl7009_charger_disable_otg,
	.is_enabled = hl7009_charger_vbus_is_enabled,
};

static const struct regulator_desc hl7009_charger_vbus_desc = {
	.name = "charger_otg_vbus",
	.of_match = "charger_otg_vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &hl7009_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
hl7009_charger_register_vbus_regulator(struct hl7009_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &hl7009_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		ZTE_CM_IC_ERROR("Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
hl7009_charger_register_vbus_regulator(struct hl7009_charger_info *info)
{
	return 0;
}
#endif

static int hl7009_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct hl7009_charger_info *info =
		container_of(nb, struct hl7009_charger_info, usb_notify);

	ZTE_CM_IC_INFO("limit: %d\n", limit);

	info->limit = limit;

#ifdef CONFIG_VENDOR_SQC_CHARGER
	sqc_notify_daemon_changed(SQC_NOTIFY_USB,
					SQC_NOTIFY_USB_STATUS_CHANGED, !!limit);
#endif

	return NOTIFY_OK;
}

static void hl7009_print_regs(struct hl7009_charger_info *info, char *buffer, unsigned int len)
{
	int i = 0;
	u8 value[HL7009_REG_NUM] = {0};
	char temp = 0;

	for (i = 0; i < ARRAY_SIZE(value); i++) {
		hl7009_read(info, reg_tab[i].addr, &(value[i]));
		if (i == ARRAY_SIZE(value) - 1) {
			ZTE_CM_IC_INFO("0x00=0x%02x,0x01=0x%02x,0x02=0x%02x,0x03=0x%02x,0x04=0x%02x,0x05=0x%02x,0x06=0x%02x,"
				"0x07=0x%02x,0x08=0x%02x,0x09=0x%02x,0x0A=0x%02x,0x0B=0x%02x,0x0C=0x%02x,\n",
				value[0], value[1], value[2], value[3], value[4], value[5],
				value[6], value[7], value[8], value[9], value[10], value[11], value[12]);
		}
	}

	temp = (value[9] & GENMASK(3, 1)) >> 1;

	switch (temp) {
	case 0:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: NOT CHARGING, ");
		break;
	case 1:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: PRE CHARGING, ");
		break;
	case 2:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: CC, ");
		break;
	case 3:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: CV, ");
		break;
	case 4:
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "CHG_STAT: CHARGER TERMINATION, ");
		break;
	default:
		break;
	}

	if (value[12] & BIT(7)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "WT_FAULT: 1, ");
	}

	if (value[10] & BIT(5)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 1, ");
		info->chg_status |= BIT(SQC_CHG_STATUS_AICL);
	} else {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "VINDPM: 0, ");
		info->chg_status &= ~ BIT(SQC_CHG_STATUS_AICL);
	}

	if (value[10] & BIT(6)) {
		snprintf(buffer + strlen(buffer), len - strlen(buffer), "IINDPM: 1, ");
	}
}

static int hl7009_charger_dumper_reg(struct hl7009_charger_info *info)
{
	int usb_icl = 0, fcc = 0, fcv = 0, topoff = 0, recharge_voltage = 0;
	char buffer[512] = {0, };

	hl7009_charger_get_termina_vol(info, &fcv);

	hl7009_charger_get_limit_current(info, &usb_icl);

	hl7009_charger_get_current(info, &fcc);

	hl7009_charger_get_termina_cur(info, &topoff);

	hl7009_charger_get_recharging_vol(info, &recharge_voltage);
	
	hl7009_print_regs(info, buffer, sizeof(buffer));

	ZTE_CM_IC_INFO("charging[%d], fcv[%d], usb_icl[%d], fcc[%d], topoff[%d], rechg_volt[%d], %s",
				info->charging, fcv / 1000, usb_icl / 1000, fcc / 1000,
				topoff / 1000, recharge_voltage / 1000, buffer);
	return 0;
}

static void hl7009_charger_detect_status(struct hl7009_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	ZTE_CM_IC_INFO("chg_state: %d", info->usb_phy->chg_state);
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	ZTE_CM_IC_INFO("limit %d", min);
	return;
}

#ifdef CONFIG_ZTE_POWER_SUPPLY_COMMON
#ifndef CONFIG_FAST_CHARGER_SC27XX
static int zte_sqc_set_prop_by_name(const char *name, enum zte_power_supply_property psp, int data)
{
	struct zte_power_supply *psy = NULL;
	union power_supply_propval val = {0, };
	int rc = 0;

	if (name == NULL) {
		ZTE_CM_IC_INFO("psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = zte_power_supply_get_by_name(name);
	if (!psy) {
		ZTE_CM_IC_INFO("get %s psy failed!!\n", name);
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
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

	*charing_status = info->chg_status;

	return 0;
}

static int sqc_mp_set_enable_chging(void *arg, unsigned int en)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

	ZTE_CM_IC_INFO("%d\n", en);

	if (en && info->enable_powerptah) {
		ZTE_CM_IC_INFO(" powerptah enabled, force disable chging\n");
		en = false;
	}

	if (en) {
		/*we should disable watchdog timer for HL7009,otherwise FCV reset can cause IC damage when Vbat > FCV*/
		hl7009_charger_set_watchdog_timer(info, 0);
		hl7009_charger_feed_watchdog(info);

		hl7009_charger_start_charge(info);

		hl7009_charger_set_vindpm(info, 4600);

		info->charging = true;

		hl7009_charger_dumper_reg(info);
	} else {
		hl7009_charger_set_watchdog_timer(info, 0);
		hl7009_charger_stop_charge(info);
		hl7009_charger_dumper_reg(info);
		info->charging = false;
	}

	return 0;
}

static int sqc_mp_get_enable_chging(void *arg, unsigned int *en)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_charge_status(info, en);

	ZTE_CM_IC_INFO("%d\n", *en);

	return ret;
}

static int sqc_mp_get_ichg(void *arg, u32 *ichg_ma)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_current(info, ichg_ma);

	ZTE_CM_IC_INFO("%d\n", *ichg_ma);

	return ret;
}

static int sqc_mp_set_ichg(void *arg, u32 mA)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_set_current(info, mA);

	ZTE_CM_IC_INFO("%d\n", mA);

	return ret;
}

static int sqc_mp_set_ieoc(void *arg, u32 mA)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_set_termina_cur(info, mA);

	ZTE_CM_IC_INFO("%d\n", mA);

	return ret;
}

static int sqc_mp_get_ieoc(void *arg, u32 *ieoc)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_termina_cur(info, ieoc);

	ZTE_CM_IC_INFO("%d\n", *ieoc);

	return ret;
}

static int sqc_mp_get_aicr(void *arg, u32 *aicr_ma)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("%d\n", *aicr_ma);

	return ret;
}

static int sqc_mp_set_aicr(void *arg, u32 aicr_ma)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_set_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("%d\n", aicr_ma);

	return ret;
}

static int sqc_mp_get_cv(void *arg, u32 *cv_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("%d\n", *cv_mv);

	return ret;
}

static int sqc_mp_set_cv(void *arg, u32 cv_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	if (info->term_vol != cv_mv) {
		ret = hl7009_charger_set_hiz(info, true);
		if (ret) {
			ZTE_CM_IC_ERROR("set HIZ failed!\n");
		} else {
			ret = hl7009_charger_set_termina_vol(info, cv_mv);
			if (ret) {
				ZTE_CM_IC_ERROR("set terminal vol failed\n");
			}
			info->term_vol = cv_mv;
			ZTE_CM_IC_INFO("%d\n", cv_mv);
			hl7009_charger_set_hiz(info, false);
		}
	}
	return ret;
}

static int sqc_mp_get_rechg_voltage(void *arg, u32 *rechg_volt_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("%d\n", *rechg_volt_mv);

	return ret;
}

static int sqc_mp_set_rechg_voltage(void *arg, u32 rechg_volt_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_set_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("%d\n", rechg_volt_mv);

	return ret;
}

static int sqc_mp_ovp_volt_get(void *arg, u32 *ac_ovp_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

    *ac_ovp_mv = info->vbus_ovp_mv;

    ZTE_CM_IC_INFO("%dmV\n", *ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_ovp_volt_set(void *arg, u32 ac_ovp_mv)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

	info->vbus_ovp_mv = ac_ovp_mv;
	
	ZTE_CM_IC_INFO("%d mV\n", info->vbus_ovp_mv);
	
	hl7009_charger_set_ovp(info, info->vbus_ovp_mv);

	return 0;
}

static int sqc_mp_get_vbat(void *arg, u32 *mV)
{
	union power_supply_propval batt_vol_uv;
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(HL7009_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", HL7009_FGU_NAME);
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

	fuel_gauge = power_supply_get_by_name(HL7009_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", HL7009_FGU_NAME);
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
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;
	union power_supply_propval val = {};
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(HL7009_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", HL7009_FGU_NAME);
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
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

	info->enable_powerptah = !!enabled;

	return 0;
}

static int sqc_enable_powerpath_get(void *arg, unsigned int *enabled)
{
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)arg;

	*enabled = info->enable_powerptah;

	return 0;
}

static int sqc_enable_hiz_set(void *arg, unsigned int enable)
{
	struct hl7009_charger_info *chg_dev = (struct hl7009_charger_info *)arg;

	hl7009_charger_set_hiz(chg_dev, !!enable);

	return 0;
}

static int sqc_enable_hiz_get(void *arg, unsigned int *enable)
{
	struct hl7009_charger_info *chg_dev = (struct hl7009_charger_info *)arg;
	int ret = 0;

	ret = hl7009_charger_get_hiz(chg_dev, enable);

	return ret;
}

static struct sqc_pmic_chg_ops hl7009_sqc_chg_ops = {

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
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)sqc_bc1d2_proto_node.arg;

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
	struct hl7009_charger_info *info = (struct hl7009_charger_info *)sqc_bc1d2_proto_node.arg;

	sscanf(val, "%d", &sleep_mode_enable);

	ZTE_CM_IC_INFO("sleep_mode_enable = %d\n", sleep_mode_enable);

	if (sleep_mode_enable) {
		if (sqc_chg_type != SQC_SLEEP_MODE_TYPE) {
			ZTE_CM_IC_INFO("sleep on status");

			/*disabel sqc-daemon*/
			sqc_chg_type = SQC_SLEEP_MODE_TYPE;
			sqc_notify_daemon_changed(SQC_NOTIFY_USB, SQC_NOTIFY_USB_STATUS_CHANGED, 1);

			ret = hl7009_charger_set_watchdog_timer(info, 0);
			if (ret < 0)
				ZTE_CM_IC_ERROR("failed to set watchdog timer\n");

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

static int hl7009_charger_hw_init(struct hl7009_charger_info *info)
{
    struct sprd_battery_info bat_info = { };
    int voltage_max_microvolt = 4400, term_current_ma = 100;
    int ret = 0;

    bat_info.batt_id_cha = NULL;

    if (info->psy_usb)
#ifdef CONFIG_VENDOR_SQC_CHARGER
        ret = sprd_battery_get_battery_info_sqc(info->psy_usb, &bat_info, 0);
#else
        ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
#endif
    if (ret || !info->psy_usb) {
        ZTE_CM_IC_ERROR("no battery information is supplied\n");

         /*
         * If no battery information is supplied, we should set
         * default charge termination current to 100 mA, and default
         * charge termination voltage to 4.2V.
         */
        info->cur.sdp_limit = 500000;
        info->cur.sdp_cur = 500000;
        info->cur.dcp_limit = 5000000;
        info->cur.dcp_cur = 500000;
        info->cur.cdp_limit = 5000000;
        info->cur.cdp_cur = 1500000;
        info->cur.unknown_limit = 5000000;
        info->cur.unknown_cur = 500000;
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
        term_current_ma = bat_info.charge_term_current_ua / 1000;
        sprd_battery_put_battery_info(info->psy_usb, &bat_info);
    }

/*set enable HIZ begin*/
    hl7009_charger_set_hiz(info, true);
/*set enable HIZ end*/
    ret = hl7009_update_bits(info, HL7009_REG_B, HL7009_REG_RST_MASK,
                              HL7009_REG_RST_MASK);
    if (ret) {
        hl7009_charger_set_hiz(info, false);
        ZTE_CM_IC_ERROR("reset failed\n");
        return ret;
    }

    ret = hl7009_update_bits(info, HL7009_REG_1, HL7009_REG_FSEL_MASK, (0x01 << HL7009_REG_FSEL_SHIFT));
    if (ret) {
        ZTE_CM_IC_ERROR("set hl7009 switch freq failed ret = %d\n", ret);
        return ret;
    }

    ret = hl7009_update_bits(info, HL7009_REG_1, HL7009_REG_WD_TMR_MASK, 0);
    if (ret) {
        ZTE_CM_IC_ERROR("set WATCHDOG disable failed ret = %d\n", ret);
        return ret;
    }

    ret = hl7009_update_bits(info, HL7009_REG_4, HL7009_REG_EN_TERM_MASK,
                              HL7009_REG_EN_TERM_MASK);
    if (ret) {
        ZTE_CM_IC_ERROR("en_term failed\n");
        return ret;
    }

    ret = hl7009_charger_set_treg(info, HL7009_TREG_110C);
    if (ret) {
        ZTE_CM_IC_ERROR("set hl7009 treg failed\n");
        return ret;
    }

    ret = hl7009_charger_set_ovp(info, HL7009_FCHG_OVP_9V);
    if (ret) {
        ZTE_CM_IC_ERROR("set hl7009 ovp failed\n");
        return ret;
    }

    ret = hl7009_charger_set_vindpm(info, 4600);
    if (ret) {
        ZTE_CM_IC_ERROR("set vindpm vol failed\n");
        return ret;
    }

    hl7009_charger_set_hiz(info, true);
    ret = hl7009_charger_set_termina_vol(info, voltage_max_microvolt);
    if (ret) {
        ZTE_CM_IC_ERROR("set terminal vol failed\n");
        return ret;
    }
    info->term_vol = voltage_max_microvolt;
    hl7009_charger_set_hiz(info, false);

    ret = hl7009_charger_set_termina_cur(info, term_current_ma);
    if (ret) {
        ZTE_CM_IC_ERROR("set terminal cur failed\n");
        return ret;
    }

    ret = hl7009_charger_set_limit_current(info, 1000);
    if (ret)
        ZTE_CM_IC_ERROR("set limit current failed\n");

    ret = hl7009_charger_set_watchdog_timer(info, 0);
    if (ret)
        ZTE_CM_IC_ERROR("set watchdog timer failed\n");

	hl7009_charger_dumper_reg(info);

    return ret;

}

static int hl7009_get_chip_id(struct hl7009_charger_info *info)
{
	u8 reg_value = 0;
	int ret = 0;

	ret = hl7009_read(info, HL7009_REG_3, &reg_value);
	if (ret < 0) {
		ZTE_CM_IC_INFO(" read id failed, ret = %d\n", ret);
		return ret;
	} else if((reg_value & 0xff) != 0x71) {
		ZTE_CM_IC_INFO("chip id = 0x%02X\n", (reg_value & 0xff));
		return -1;
	} else {
		ZTE_CM_IC_INFO(" id correct,found HL7009!\n");
	}

	return 0;
}

static int hl7009_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct hl7009_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	struct sqc_pmic_chg_ops *sqc_ops = NULL;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		ZTE_CM_IC_ERROR("No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	ZTE_CM_IC_INFO("initializing...\n");

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	ret = hl7009_get_chip_id(info);
	if (ret < 0) {
		ZTE_CM_IC_ERROR("get id failed\n");
		return -ENODEV;
	}

	alarm_init(&info->wdg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);

	i2c_set_clientdata(client, info);
	power_path_control(info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		ZTE_CM_IC_ERROR("failed to find USB phy\n");
		goto err_mutex_lock;
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		ZTE_CM_IC_ERROR("failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = hl7009_charger_register_vbus_regulator(info);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register vbus regulator.\n");
		return ret;
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
		info->charger_pd_mask = HL7009_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = HL7009_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = HL7009_DISABLE_PIN_MASK_2720;
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

	info->usb_notify.notifier_call = hl7009_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register notifier:%d\n", ret);
		goto err_mutex_lock;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &hl7009_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		ZTE_CM_IC_ERROR("failed to register power supply\n");
		goto err_usb_notifier;
	}

	ret = hl7009_charger_hw_init(info);
	if (ret)
		return ret;

	proc_create_data("driver/chg_name", 0664, NULL, &hl7009_chip_name_node, info);

	hl7009_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);

	/*zte add to get limit value*/
	hl7009_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, hl7009_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  hl7009_charger_feed_watchdog_work);

	sqc_ops = kzalloc(sizeof(struct sqc_pmic_chg_ops), GFP_KERNEL);
	memcpy(sqc_ops, &hl7009_sqc_chg_ops, sizeof(struct sqc_pmic_chg_ops));
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

	ZTE_CM_IC_INFO("successful!\n");

	return 0;

err_usb_notifier:
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_mutex_lock:
	mutex_destroy(&info->lock);
	return ret;
}

static void hl7009_charger_shutdown(struct i2c_client *client)
{
	struct hl7009_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->need_reinit = true;
		cancel_delayed_work_sync(&info->otg_work);
		ret = hl7009_update_bits(info, HL7009_REG_1,
					  HL7009_REG_OTG_EN_MASK,
					  0);
		if (ret)
			ZTE_CM_IC_ERROR("disable hl7009 otg failed ret = %d\n", ret);

		ZTE_CM_IC_INFO("disable otg func when shutdown\n");
		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			ZTE_CM_IC_ERROR("enable charger detection function failed ret = %d\n", ret);
	}
}

static int hl7009_charger_remove(struct i2c_client *client)
{
	struct hl7009_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int hl7009_charger_alarm_prepare(struct device *dev)
{
	struct hl7009_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;

	if (!info) {
		ZTE_CM_IC_ERROR("info is null!\n");
		return 0;
	}

	if (!info->otg_enable)
		return 0;

	now = ktime_get_boottime();
	add = ktime_set(HL7009_WDG_TIMER_S, 0);
	alarm_start(&info->wdg_timer, ktime_add(now, add));
	return 0;
}

static void hl7009_charger_alarm_complete(struct device *dev)
{
	struct hl7009_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return;
	}

	if (!info->otg_enable)
		return;

	alarm_cancel(&info->wdg_timer);
}

static int hl7009_charger_suspend(struct device *dev)
{
	struct hl7009_charger_info *info = dev_get_drvdata(dev);

	if (info->otg_enable)
		/* feed watchdog first before suspend */
		hl7009_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	info->suspended = true;

	cancel_delayed_work_sync(&info->wdt_work);

	return 0;
}

static int hl7009_charger_resume(struct device *dev)
{
	struct hl7009_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

	if (info->otg_enable)
		/* feed watchdog first before suspend */
		hl7009_charger_feed_watchdog(info);

	if (!info->otg_enable)
		return 0;

	info->suspended = false;

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops hl7009_charger_pm_ops = {
	.prepare = hl7009_charger_alarm_prepare,
	.complete = hl7009_charger_alarm_complete,
	SET_SYSTEM_SLEEP_PM_OPS(hl7009_charger_suspend,
				hl7009_charger_resume)
};

static const struct i2c_device_id hl7009_i2c_id[] = {
	{"hl7009_chg", 0},
	{}
};

static const struct of_device_id hl7009_charger_of_match[] = {
	{ .compatible = "halo,hl7009", },
	{ }
};

MODULE_DEVICE_TABLE(of, hl7009_charger_of_match);

static struct i2c_driver hl7009_charger_driver = {
	.driver = {
		.name = "hl7009_chg",
		.of_match_table = hl7009_charger_of_match,
		.pm = &hl7009_charger_pm_ops,
	},
	.probe = hl7009_charger_probe,
	.shutdown = hl7009_charger_shutdown,
	.remove = hl7009_charger_remove,
	.id_table = hl7009_i2c_id,
};

module_i2c_driver(hl7009_charger_driver);
MODULE_DESCRIPTION("HL7009 Charger Driver");
MODULE_LICENSE("GPL v2");
