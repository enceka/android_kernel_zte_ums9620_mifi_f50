/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define CHARGER_IC_TAG   "[ZTE_LDD_CHARGE][CHARGER_IC][sc8989x]"

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


#define SC8989X_DRV_VERSION             "1.0.0_SC"

#define SC8989X_REG_00                  0x00
#define REG00_EN_HIZ_MASK               BIT(7)
#define REG00_EN_HIZ_SHIFT              7
#define REG00_EN_HIZ                    1
#define REG00_EXIT_HIZ                  0
#define REG00_EN_ILIM_MASK              BIT(6)
#define REG00_EN_ILIM_SHIFT             6
#define REG00_EN_ILIM_DISABLE           0
#define REG00_IINDPM_MASK               GENMASK(5, 0)
#define REG00_IINDPM_SHIFT              0
#define REG00_IINDPM_BASE               100
#define REG00_IINDPM_LSB                50
#define REG00_IINDPM_MIN                100
#define REG00_IINDPM_MAX                3250

#define SC8989X_REG_01                  0x01

#define SC8989X_REG_02                  0x02

#define SC8989X_REG_03                  0x03
#define REG03_WDT_RST_MASK              BIT(6)
#define REG03_WDT_RST_SHIFT             6
#define REG03_WDT_RESET                 1
#define REG03_OTG_MASK                  BIT(5)
#define REG03_OTG_SHIFT                 5
#define REG03_OTG_ENABLE                1
#define REG03_OTG_DISABLE               0
#define REG03_CHG_MASK                  BIT(4)
#define REG03_CHG_SHIFT                 4
#define REG03_CHG_ENABLE                1
#define REG03_CHG_DISABLE               0


#define SC8989X_REG_04                  0x04
#define REG04_ICC_MASK                  GENMASK(6, 0)
#define REG04_ICC_SHIFT                 0
#define REG04_ICC_BASE                  0
#define REG04_ICC_LSB                   60
#define REG04_ICC_MIN                   0
#define REG04_ICC_MAX                   5040

#define SC8989X_REG_05                  0x05
#define REG05_ITC_MASK                  GENMASK(7, 4)
#define REG05_ITC_SHIFT                 4
#define REG05_ITC_BASE                  60
#define REG05_ITC_LSB                   60
#define REG05_ITC_MIN                   60
#define REG05_ITC_MAX                   960
#define REG05_ITERM_MASK                GENMASK(3, 0)
#define REG05_ITERM_SHIFT               0
#define REG05_ITERM_BASE                30
#define REG05_ITERM_LSB                 60
#define REG05_ITERM_MIN                 30
#define REG05_ITERM_MAX                 960

#define SC8989X_REG_06                  0x06
#define REG06_VREG_MASK                 GENMASK(7, 2)
#define REG06_VREG_SHIFT                2
#define REG06_VREG_BASE                 3840
#define REG06_VREG_LSB                  16
#define REG06_VREG_MIN                  3840
#define REG06_VREG_MAX                  4856
#define REG06_VBAT_LOW_MASK             BIT(1)
#define REG06_VBAT_LOW_SHIFT            1
#define REG06_VBAT_LOW_2P8V             0
#define REG06_VBAT_LOW_3P0V             1
#define REG06_VRECHG_MASK               BIT(0)
#define REG06_VRECHG_SHIFT              0
#define REG06_VRECHG_100MV              0
#define REG06_VRECHG_200MV              1

#define SC8989X_REG_07                  0x07
#define REG07_TWD_MASK                  GENMASK(5, 4)
#define REG07_TWD_SHIFT                 4
#define REG07_TWD_DISABLE               0
#define REG07_TWD_40S                   1
#define REG07_TWD_80S                   2
#define REG07_TWD_160S                  3
#define REG07_EN_TIMER_MASK             BIT(3)
#define REG07_EN_TIMER_SHIFT            3
#define REG07_CHG_TIMER_ENABLE          1
#define REG07_CHG_TIMER_DISABLE         0


#define SC8989X_REG_08                  0x08

#define SC8989X_REG_09                  0x09
#define REG09_BATFET_DIS_MASK           BIT(5)
#define REG09_BATFET_DIS_SHIFT          5
#define REG09_BATFET_ENABLE             0
#define REG09_BATFET_DISABLE            1

#define SC8989X_REG_0A                  0x0A
#define REG0A_BOOSTV_MASK               GENMASK(7, 4)
#define REG0A_BOOSTV_SHIFT              4
#define REG0A_BOOSTV_LIM_MASK           GENMASK(2, 0)
#define REG0A_BOOSTV_LIM_SHIFT          0
#define REG0A_BOOSTV_LIM_1P2A           0x02

#define SC8989X_REG_0B                  0x0B

#define SC8989X_REG_0C                  0x0C
#define REG0C_OTG_FAULT                 BIT(6)

#define SC8989X_REG_0D                  0x0D
#define REG0D_FORCE_VINDPM_MASK         BIT(7)
#define REG0D_FORCE_VINDPM_SHIFT        7
#define REG0D_FORCE_VINDPM_ENABLE       1

#define REG0D_VINDPM_MASK               GENMASK(6, 0)
#define REG0D_VINDPM_BASE               2600
#define REG0D_VINDPM_LSB                100
#define REG0D_VINDPM_MIN                3900
#define REG0D_VINDPM_MAX                15300

#define SC8989X_REG_0E                  0x0E
#define REG0E_ADC_VBAT_MASK             GENMASK(6, 0)
#define REG0E_ADC_VBAT_BASE             2304
#define REG0E_ADC_VBAT_LSB              20
#define REG0E_ADC_VBAT_MIN              2304
#define REG0E_ADC_VBAT_MAX              4848

#define SC8989X_REG_0F                  0x0F
#define SC8989X_REG_10                  0x10
#define SC8989X_REG_11                  0x11
#define REG11_ADC_VBUS_MASK             GENMASK(6, 0)
#define REG11_ADC_VBUS_BASE             2600
#define REG11_ADC_VBUS_LSB              100
#define REG11_ADC_VBUS_MIN              2600
#define REG11_ADC_VBUS_MAX              15300

#define SC8989X_REG_12                  0x12
#define REG12_ADC_IBAT_MASK             GENMASK(6, 0)
#define REG12_ADC_IBAT_BASE             0
#define REG12_ADC_IBAT_LSB              50
#define REG12_ADC_IBAT_MIN              0
#define REG12_ADC_IBAT_MAX              6350

#define SC8989X_REG_13                  0x13

#define SC8989X_REG_14                  0x14
#define REG14_REG_RESET_MASK            BIT(7)
#define REG14_REG_RESET_SHIFT           7
#define REG14_REG_RESET                 1
#define REG14_VENDOR_ID_MASK            GENMASK(5, 3)
#define REG14_VENDOR_ID_SHIFT           3
#define SC8989X_VENDOR_ID               4

#define SC8989X_REG_NUM                 21

/* Other Realted Definition*/
#define SC8989X_FGU_NAME            "sc27xx-fgu"

#define BIT_DP_DM_BC_ENB                BIT(0)

#define SC8989X_WDT_VALID_MS            50

#define SC8989X_WDG_TIMER_MS            15000
#define SC8989X_OTG_VALID_MS            500
#define SC8989X_OTG_RETRY_TIMES         10

#define SC8989X_DISABLE_PIN_MASK        BIT(0)
#define SC8989X_DISABLE_PIN_MASK_2721   BIT(15)

#define SC8989X_FAST_CHG_VOL_MAX        10500000
#define SC8989X_NORMAL_CHG_VOL_MAX      6500000

#define SC8989X_WAKE_UP_MS              2000

struct sc8989x_charger_sysfs {
    char *name;
    struct attribute_group attr_g;
    struct device_attribute attr_sc8989x_dump_reg;
    struct device_attribute attr_sc8989x_lookup_reg;
    struct device_attribute attr_sc8989x_sel_reg_id;
    struct device_attribute attr_sc8989x_reg_val;
    struct attribute *attrs[5];

    struct sc8989x_charger_info *info;
};

struct sc8989x_charger_info {
    struct i2c_client *client;
    struct device *dev;
    struct mutex lock;
    struct mutex input_limit_cur_lock;
    struct delayed_work otg_work;
    struct delayed_work wdt_work;
    struct delayed_work dump_work;
    struct regmap *pmic;
    struct alarm wdg_timer;
    struct sc8989x_charger_sysfs *sysfs;
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
};

struct sc8989x_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct sc8989x_charger_reg_tab reg_tab[SC8989X_REG_NUM + 1] = {
    {0, SC8989X_REG_00, "Setting Input Limit Current reg"},
    {1, SC8989X_REG_01, "Setting Vindpm_OS reg"},
    {2, SC8989X_REG_02, "Related Function Enable reg"},
    {3, SC8989X_REG_03, "Related Function Config reg"},
    {4, SC8989X_REG_04, "Setting Charge Limit Current reg"},
    {5, SC8989X_REG_05, "Setting Terminal Current reg"},
    {6, SC8989X_REG_06, "Setting Charge Limit Voltage reg"},
    {7, SC8989X_REG_07, "Related Function Config reg"},
    {8, SC8989X_REG_08, "IR Compensation Resistor Setting reg"},
    {9, SC8989X_REG_09, "Related Function Config reg"},
    {10, SC8989X_REG_0A, "Boost Mode Related Setting reg"},
    {11, SC8989X_REG_0B, "Status reg"},
    {12, SC8989X_REG_0C, "Fault reg"},
    {13, SC8989X_REG_0D, "Setting Vindpm reg"},
    {14, SC8989X_REG_0E, "ADC Conversion of Battery Voltage reg"},
    {15, SC8989X_REG_0F, "ADDC Conversion of System Voltage reg"},
    {16, SC8989X_REG_10, "ADC Conversion of TS Voltage as Percentage of REGN reg"},
    {17, SC8989X_REG_11, "ADC Conversion of VBUS voltage reg"},
    {18, SC8989X_REG_12, "ICHGR Setting reg"},
    {19, SC8989X_REG_13, "IDPM Limit Setting reg"},
    {20, SC8989X_REG_14, "Related Function Config reg"},
    {21, 0, "null"},
};

static int sc8989x_charger_set_limit_current(struct sc8989x_charger_info *info, u32 limit_cur);

static int sc8989x_read(struct sc8989x_charger_info *info, u8 reg, u8 *data)
{
    int ret = 0;

    ret = i2c_smbus_read_byte_data(info->client, reg);
    if (ret < 0)
        return ret;

    *data = ret;
    return 0;
}

static int sc8989x_write(struct sc8989x_charger_info *info, u8 reg, u8 data)
{
    return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sc8989x_update_bits(struct sc8989x_charger_info *info, u8 reg,
                               u8 mask, u8 data)
{
    u8 v;
    int ret = 0;

    ret = sc8989x_read(info, reg, &v);
    if (ret < 0)
        return ret;

    v &= ~mask;
    v |= (data & mask);

    return sc8989x_write(info, reg, v);
}

static void sc8989x_charger_dump_register(struct sc8989x_charger_info *info)
{
    int i, ret, len, idx = 0;
    u8 reg_val = 0;
    char buf[512];

    memset(buf, '\0', sizeof(buf));
    for (i = 0; i < SC8989X_REG_NUM; i++) {
        ret = sc8989x_read(info, reg_tab[i].addr, &reg_val);
        if (ret == 0) {
            len = snprintf(buf + idx, sizeof(buf) - idx,
                           "[0x%.2x]=0x%.2x; ", reg_tab[i].addr,
                           reg_val);
            idx += len;
        }
    }

    ZTE_CM_IC_INFO("%s", buf);
}

static int sc8989x_charger_is_fgu_present(struct sc8989x_charger_info *info)
{
    struct power_supply *psy;

    psy = power_supply_get_by_name(SC8989X_FGU_NAME);
    if (!psy) {
        ZTE_CM_IC_ERROR("Failed to find psy of sc27xx_fgu\n");
        return -ENODEV;
    }
    power_supply_put(psy);

    return 0;
}

static int sc8989x_charger_set_vindpm(struct sc8989x_charger_info *info, u32 vol)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_update_bits(info, SC8989X_REG_0D, REG0D_FORCE_VINDPM_MASK,
                              REG0D_FORCE_VINDPM_ENABLE << REG0D_FORCE_VINDPM_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("set force vindpm failed\n");
        return ret;
    }

    if (vol < REG0D_VINDPM_MIN)
        vol = REG0D_VINDPM_MIN;
    else if (vol > REG0D_VINDPM_MAX)
        vol = REG0D_VINDPM_MAX;
    reg_val = (vol - REG0D_VINDPM_BASE) / REG0D_VINDPM_LSB;

    return sc8989x_update_bits(info, SC8989X_REG_0D,
                               REG0D_FORCE_VINDPM_MASK, reg_val);
}

static int sc8989x_charger_set_recharging_vol(struct sc8989x_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol <= 100)
        reg_val = REG06_VRECHG_100MV;
    else
        reg_val = REG06_VRECHG_200MV;

    return sc8989x_update_bits(info, SC8989X_REG_06, REG06_VRECHG_MASK,
                               reg_val << REG06_VRECHG_SHIFT);
}

static int sc8989x_charger_get_recharging_vol(struct sc8989x_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sc8989x_read(info, SC8989X_REG_06, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG06_VRECHG_MASK) >> REG06_VRECHG_SHIFT;

    *vol = reg_val ? 200 : 100;

    return 0;
}

static int sc8989x_charger_set_hiz(struct sc8989x_charger_info *info, u32 en_hiz)
{
    u8 reg_val = 0;

    if (en_hiz)
        reg_val = REG00_EN_HIZ;
    else
        reg_val = REG00_EXIT_HIZ;

    return sc8989x_update_bits(info, SC8989X_REG_00, REG00_EN_HIZ_MASK,
                               reg_val << REG00_EN_HIZ_SHIFT);
}

static int sc8989x_charger_get_hiz(struct sc8989x_charger_info *info, u32 *en_hiz)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sc8989x_read(info, SC8989X_REG_00, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG00_EN_HIZ_MASK) >> REG00_EN_HIZ_SHIFT;

    *en_hiz = reg_val ? REG00_EN_HIZ : REG00_EXIT_HIZ;

    return 0;
}

static int sc8989x_charger_set_termina_vol(struct sc8989x_charger_info *info, u32 vol)
{
    u8 reg_val = 0;

    if (vol < REG06_VREG_MIN)
        vol = REG06_VREG_MIN;
    else if (vol >= REG06_VREG_MAX)
        vol = REG06_VREG_MAX;
    reg_val = (vol - REG06_VREG_BASE) / REG06_VREG_LSB;

    return sc8989x_update_bits(info, SC8989X_REG_06, REG06_VREG_MASK,
                               reg_val << REG06_VREG_SHIFT);
}

static int sc8989x_charger_get_termina_vol(struct sc8989x_charger_info *info, u32 *vol)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sc8989x_read(info, SC8989X_REG_03, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG06_VREG_MASK) >> REG06_VREG_SHIFT;

    *vol = reg_val * REG06_VREG_LSB + REG06_VREG_BASE;

    return 0;
}

static int sc8989x_charger_set_termina_cur(struct sc8989x_charger_info *info, u32 cur)
{
    u8 reg_val = 0;

    if (cur <= REG05_ITERM_MIN)
        cur = REG05_ITERM_MIN;
    else if (cur >= REG05_ITERM_MAX)
        cur = REG05_ITERM_MAX;
    reg_val = (cur - REG05_ITERM_BASE) / REG05_ITERM_LSB;

    return sc8989x_update_bits(info, SC8989X_REG_05, REG05_ITERM_MASK, reg_val);
}

static int sc8989x_charger_get_termina_cur(struct sc8989x_charger_info *info, u32 *cur)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sc8989x_read(info, SC8989X_REG_05, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to read termina_vol\n");
        return ret;
    }

    reg_val = (reg_val & REG05_ITERM_MASK);

    *cur = reg_val * REG05_ITERM_LSB + REG05_ITERM_BASE;

    return 0;
}


static int sc8989x_charger_hw_init(struct sc8989x_charger_info *info)
{
     int ret = 0;

    ret = sc8989x_update_bits(info, SC8989X_REG_14, REG14_REG_RESET_MASK,
                              REG14_REG_RESET << REG14_REG_RESET_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("reset failed\n");
        return ret;
    }

    ret = sc8989x_charger_set_vindpm(info, 4600);
    if (ret) {
        ZTE_CM_IC_ERROR("set vindpm vol failed\n");
        return ret;
    }

    ret = sc8989x_charger_set_termina_vol(info, 4200);
    if (ret) {
        ZTE_CM_IC_ERROR("set terminal vol failed\n");
        return ret;
    }

    ret = sc8989x_charger_set_termina_cur(info, 300);
    if (ret) {
        ZTE_CM_IC_ERROR("set terminal cur failed\n");
        return ret;
    }

    ret = sc8989x_charger_set_limit_current(info, 1000);
    if (ret)
        ZTE_CM_IC_ERROR("set limit current failed\n");

    ret = sc8989x_update_bits(info, SC8989X_REG_0A, REG0A_BOOSTV_LIM_MASK,
                              REG0A_BOOSTV_LIM_1P2A);
    if (ret) {
        ZTE_CM_IC_ERROR("set boost limit current failed\n");
        return ret;
    }

    return ret;
}


static int sc8989x_charger_start_charge(struct sc8989x_charger_info *info)
{
    int ret = 0;

    ret = sc8989x_update_bits(info, SC8989X_REG_00,
                              REG00_EN_HIZ_MASK, REG00_EXIT_HIZ);
    if (ret)
        ZTE_CM_IC_ERROR("disable HIZ mode failed\n");

    ret = sc8989x_update_bits(info, SC8989X_REG_07, REG07_TWD_MASK,
                              REG07_TWD_40S << REG07_TWD_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to enable watchdog\n");
        return ret;
    }

    ret = regmap_update_bits(info->pmic, info->charger_pd,
                             info->charger_pd_mask, 0);
    if (ret) {
        ZTE_CM_IC_ERROR("enable charge failed\n");
        return ret;
    }

    ret = sc8989x_update_bits(info, SC8989X_REG_03,
                              REG03_OTG_MASK, REG03_OTG_DISABLE);
    if (ret) {
        ZTE_CM_IC_ERROR("disable otg failed\n");
    }

    ret = sc8989x_update_bits(info, SC8989X_REG_03, REG03_CHG_MASK,
                              REG03_CHG_ENABLE << REG03_CHG_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("disable otg failed\n");
    }

    return ret;
}

static int sc8989x_charger_stop_charge(struct sc8989x_charger_info *info)
{
    int ret = 0;

    ret = regmap_update_bits(info->pmic, info->charger_pd,
                             info->charger_pd_mask,
                             info->charger_pd_mask);
    if (ret)
        ZTE_CM_IC_ERROR("disable charge failed\n");

    ret = sc8989x_update_bits(info, SC8989X_REG_07, REG07_TWD_MASK,
                              REG07_TWD_DISABLE);
    if (ret)
        ZTE_CM_IC_ERROR("Failed to disable watchdog\n");
    
    return ret;
}

static int sc8989x_charger_get_charge_status(struct sc8989x_charger_info *info, u32 *chg_enable)
{
    int ret = 0;
    u8 reg_val = 0;

    ret = sc8989x_read(info, SC8989X_REG_03, &reg_val);
    if (ret)
        ZTE_CM_IC_ERROR("Failed to read charge_status\n");

    *chg_enable = (reg_val & REG03_CHG_MASK) ? 1 : 0;

    return ret;
}

static int sc8989x_charger_set_current(struct sc8989x_charger_info *info,
                                       u32 cur)
{
    u8 reg_val = 0;
    int ret = 0;

    if (cur <= REG04_ICC_MIN) {
        cur = REG04_ICC_MIN;
    } else if (cur >= REG04_ICC_MAX) {
        cur = REG04_ICC_MAX;
    }

    reg_val = cur / REG04_ICC_LSB;

    ret = sc8989x_update_bits(info, SC8989X_REG_04, REG04_ICC_MASK, reg_val);
    ZTE_CM_IC_ERROR("current = %d, reg_val = %x\n", cur, reg_val);

    return ret;
}

static int sc8989x_charger_get_current(struct sc8989x_charger_info *info,
                                       u32 *cur)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_read(info, SC8989X_REG_04, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG04_ICC_MASK;
    *cur = reg_val * REG04_ICC_LSB;

    return 0;
}

static u32 sc8989x_charger_get_limit_current(struct sc8989x_charger_info *info,
        u32 *limit_cur)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_read(info, SC8989X_REG_00, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG00_IINDPM_MASK;
    *limit_cur = reg_val * REG00_IINDPM_LSB + REG00_IINDPM_BASE;
    if (*limit_cur >= REG00_IINDPM_MAX)
        *limit_cur = REG00_IINDPM_MAX;

    return 0;
}

static int sc8989x_charger_set_limit_current(struct sc8989x_charger_info *info,
        u32 limit_cur)
{
    u8 reg_val = 0;
    int ret = 0;

    mutex_lock(&info->input_limit_cur_lock);
/*
    if (enable) {
        ret = sc8989x_charger_get_limit_current(info, &limit_cur);
        if (ret) {
            ZTE_CM_IC_ERROR("get limit cur failed\n");
            goto out;
        }

        if (limit_cur == info->actual_limit_current)
            goto out;
        limit_cur = info->actual_limit_current;
    }
*/
    ret = sc8989x_update_bits(info, SC8989X_REG_00, REG00_EN_ILIM_MASK,
                              REG00_EN_ILIM_DISABLE);
    if (ret) {
        ZTE_CM_IC_ERROR("disable en_ilim failed\n");
        goto out;
    }

    if (limit_cur >= REG00_IINDPM_MAX)
        limit_cur = REG00_IINDPM_MAX;
    if (limit_cur <= REG00_IINDPM_MIN)
        limit_cur = REG00_IINDPM_MIN;

    reg_val = (limit_cur - REG00_IINDPM_BASE) / REG00_IINDPM_LSB;
    info->actual_limit_current =
        (reg_val * REG00_IINDPM_LSB + REG00_IINDPM_BASE);
    ret = sc8989x_update_bits(info, SC8989X_REG_00, REG00_IINDPM_MASK, reg_val);
    if (ret)
        ZTE_CM_IC_ERROR("set limit cur failed\n");

    ZTE_CM_IC_INFO("set limit current reg_val = %#x, actual_limit_cur = %d\n",
             reg_val, info->actual_limit_current);

out:
    mutex_unlock(&info->input_limit_cur_lock);

    return ret;
}
/*
static u32 sc8989x_charger_get_adc_vbat(struct sc8989x_charger_info *info, u32 *vbat_mv)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_read(info, SC8989X_REG_0E, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG0E_ADC_VBAT_MASK;
    *vbat_mv = reg_val * REG0E_ADC_VBAT_LSB + REG0E_ADC_VBAT_BASE;

    if (*vbat_mv >= REG0E_ADC_VBAT_MAX)
        *vbat_mv = REG0E_ADC_VBAT_MAX;
    else if (*vbat_mv <= REG0E_ADC_VBAT_MIN)
        *vbat_mv = REG0E_ADC_VBAT_MIN;

    return 0;
}

static u32 sc8989x_charger_get_adc_ibat(struct sc8989x_charger_info *info, u32 *ibat_ma)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_read(info, SC8989X_REG_12, &reg_val);
    if (ret < 0)
        return ret;

    reg_val &= REG12_ADC_IBAT_MASK;
    *ibat_ma = reg_val * REG12_ADC_IBAT_LSB + REG12_ADC_IBAT_BASE;

    if (*ibat_ma >= REG12_ADC_IBAT_MAX)
        *ibat_ma = REG12_ADC_IBAT_MAX;
    else if (*ibat_ma <= REG12_ADC_IBAT_MIN)
        *ibat_ma = REG12_ADC_IBAT_MIN;

    return 0;
}

static u32 sc8989x_charger_get_adc_vbus(struct sc8989x_charger_info *info, u32 *vbus_mv)
{
    u8 reg_val = 0;
    int ret = 0;

    ret = sc8989x_read(info, SC8989X_REG_11, &reg_val);
    if (ret) {
        ZTE_CM_IC_ERROR("get_adc_vbus failed\n");
        return ret;
    }

    reg_val &= REG11_ADC_VBUS_MASK;
    *vbus_mv = reg_val * REG11_ADC_VBUS_LSB + REG11_ADC_VBUS_BASE;

    if (*vbus_mv >= REG11_ADC_VBUS_MAX)
        *vbus_mv = REG11_ADC_VBUS_MAX;
    else if (*vbus_mv <= REG11_ADC_VBUS_MIN)
        *vbus_mv = REG11_ADC_VBUS_MIN;

    return 0;
}
*/
static int sc8989x_charger_feed_watchdog(struct sc8989x_charger_info *info)
{
    int ret = 0;

    ret = sc8989x_update_bits(info, SC8989X_REG_03, REG03_WDT_RST_MASK,
                              REG03_WDT_RESET << REG03_WDT_RST_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("reset failed\n");
        return ret;
    }

    return ret;
}

static ssize_t sc8989x_reg_val_show(struct device *dev,
                                    struct device_attribute *attr,
                                    char *buf)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_reg_val);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;
    u8 val;
    int ret = 0;

    if (!info)
        return sprintf(buf, "%s sc8989x_sysfs->info is null\n", __func__);

    ret = sc8989x_read(info, reg_tab[info->reg_id].addr, &val);
    if (ret) {
        ZTE_CM_IC_ERROR("fail to get sc8989x_REG_0x%.2x value, ret = %d\n",
                reg_tab[info->reg_id].addr, ret);
        return sprintf(buf, "fail to get sc8989x_REG_0x%.2x value\n",
                       reg_tab[info->reg_id].addr);
    }

    return sprintf(buf, "sc8989x_REG_0x%.2x = 0x%.2x\n",
                   reg_tab[info->reg_id].addr, val);
}

static ssize_t sc8989x_reg_val_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_reg_val);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;
    u8 val;
    int ret = 0;

    if (!info) {
        ZTE_CM_IC_ERROR("sc8989x_sysfs->info is null\n");
        return count;
    }

    ret =  kstrtou8(buf, 16, &val);
    if (ret) {
        ZTE_CM_IC_ERROR("fail to get addr, ret = %d\n", ret);
        return count;
    }

    ret = sc8989x_write(info, reg_tab[info->reg_id].addr, val);
    if (ret) {
        ZTE_CM_IC_ERROR("fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
                val, reg_tab[info->reg_id].addr, ret);
        return count;
    }

    ZTE_CM_IC_INFO("wite 0x%.2x to REG_0x%.2x success\n", val,
             reg_tab[info->reg_id].addr);
    return count;
}

static ssize_t sc8989x_reg_id_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_sel_reg_id);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;
    int ret, id;

    if (!info) {
        ZTE_CM_IC_ERROR("sc8989x_sysfs->info is null\n");
        return count;
    }

    ret =  kstrtoint(buf, 10, &id);
    if (ret) {
        ZTE_CM_IC_ERROR("%s store register id fail\n", sc8989x_sysfs->name);
        return count;
    }

    if (id < 0 || id >= SC8989X_REG_NUM) {
        ZTE_CM_IC_ERROR("%s store register id fail, id = %d is out of range\n",
                sc8989x_sysfs->name, id);
        return count;
    }

    info->reg_id = id;

    ZTE_CM_IC_INFO("%s store register id = %d success\n", sc8989x_sysfs->name, id);
    return count;
}

static ssize_t sc8989x_reg_id_show(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_sel_reg_id);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;

    if (!info)
        return sprintf(buf, "%s sc8989x_sysfs->info is null\n", __func__);

    return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t sc8989x_reg_table_show(struct device *dev,
                                      struct device_attribute *attr,
                                      char *buf)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_lookup_reg);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;
    int i, len, idx = 0;
    char reg_tab_buf[2000];

    if (!info)
        return sprintf(buf, "%s sc8989x_sysfs->info is null\n", __func__);

    memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
    len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
                   "Format: [id] [addr] [desc]\n");
    idx += len;

    for (i = 0; i < SC8989X_REG_NUM; i++) {
        len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
                       "[%d] [REG_0x%.2x] [%s]; \n",
                       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
        idx += len;
    }

    return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t sc8989x_dump_reg_show(struct device *dev,
                                     struct device_attribute *attr,
                                     char *buf)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs =
        container_of(attr, struct sc8989x_charger_sysfs,
                     attr_sc8989x_dump_reg);
    struct sc8989x_charger_info *info = sc8989x_sysfs->info;

    if (!info)
        return sprintf(buf, "%s sc8989x_sysfs->info is null\n", __func__);

    sc8989x_charger_dump_register(info);

    return sprintf(buf, "%s\n", sc8989x_sysfs->name);
}

static int sc8989x_register_sysfs(struct sc8989x_charger_info *info)
{
    struct sc8989x_charger_sysfs *sc8989x_sysfs;
    int ret = 0;

    sc8989x_sysfs = devm_kzalloc(info->dev, sizeof(*sc8989x_sysfs), GFP_KERNEL);
    if (!sc8989x_sysfs)
        return -ENOMEM;

    info->sysfs = sc8989x_sysfs;
    sc8989x_sysfs->name = "sc8989x_sysfs";
    sc8989x_sysfs->info = info;
    sc8989x_sysfs->attrs[0] = &sc8989x_sysfs->attr_sc8989x_dump_reg.attr;
    sc8989x_sysfs->attrs[1] = &sc8989x_sysfs->attr_sc8989x_lookup_reg.attr;
    sc8989x_sysfs->attrs[2] = &sc8989x_sysfs->attr_sc8989x_sel_reg_id.attr;
    sc8989x_sysfs->attrs[3] = &sc8989x_sysfs->attr_sc8989x_reg_val.attr;
    sc8989x_sysfs->attrs[4] = NULL;
    sc8989x_sysfs->attr_g.name = "debug";
    sc8989x_sysfs->attr_g.attrs = sc8989x_sysfs->attrs;

    sysfs_attr_init(&sc8989x_sysfs->attr_sc8989x_dump_reg.attr);
    sc8989x_sysfs->attr_sc8989x_dump_reg.attr.name = "sc8989x_dump_reg";
    sc8989x_sysfs->attr_sc8989x_dump_reg.attr.mode = 0444;
    sc8989x_sysfs->attr_sc8989x_dump_reg.show = sc8989x_dump_reg_show;

    sysfs_attr_init(&sc8989x_sysfs->attr_sc8989x_lookup_reg.attr);
    sc8989x_sysfs->attr_sc8989x_lookup_reg.attr.name = "sc8989x_lookup_reg";
    sc8989x_sysfs->attr_sc8989x_lookup_reg.attr.mode = 0444;
    sc8989x_sysfs->attr_sc8989x_lookup_reg.show = sc8989x_reg_table_show;

    sysfs_attr_init(&sc8989x_sysfs->attr_sc8989x_sel_reg_id.attr);
    sc8989x_sysfs->attr_sc8989x_sel_reg_id.attr.name = "sc8989x_sel_reg_id";
    sc8989x_sysfs->attr_sc8989x_sel_reg_id.attr.mode = 0644;
    sc8989x_sysfs->attr_sc8989x_sel_reg_id.show = sc8989x_reg_id_show;
    sc8989x_sysfs->attr_sc8989x_sel_reg_id.store = sc8989x_reg_id_store;

    sysfs_attr_init(&sc8989x_sysfs->attr_sc8989x_reg_val.attr);
    sc8989x_sysfs->attr_sc8989x_reg_val.attr.name = "sc8989x_reg_val";
    sc8989x_sysfs->attr_sc8989x_reg_val.attr.mode = 0644;
    sc8989x_sysfs->attr_sc8989x_reg_val.show = sc8989x_reg_val_show;
    sc8989x_sysfs->attr_sc8989x_reg_val.store = sc8989x_reg_val_store;

    ret = sysfs_create_group(&info->dev->kobj, &sc8989x_sysfs->attr_g);
    if (ret < 0)
        ZTE_CM_IC_ERROR("Cannot create sysfs , ret = %d\n", ret);

    return ret;
}

static int sc8989x_charger_usb_get_property(struct power_supply *psy,
        enum power_supply_property psp,
        union power_supply_propval *val)
{
	val->intval = 0;

	return 0;
}

static int sc8989x_charger_usb_set_property(struct power_supply *psy,
        enum power_supply_property psp,
        const union power_supply_propval *val)
{
	return 0;
}

static int sc8989x_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property sc8989x_usb_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_CALIBRATE,
};

static const struct power_supply_desc sc8989x_charger_desc = {
    .name            = "sc8989x_charger",
    .type            = POWER_SUPPLY_TYPE_USB,
    .properties        = sc8989x_usb_props,
    .num_properties        = ARRAY_SIZE(sc8989x_usb_props),
    .get_property        = sc8989x_charger_usb_get_property,
    .set_property        = sc8989x_charger_usb_set_property,
    .property_is_writeable    = sc8989x_charger_property_is_writeable,
};


static void sc8989x_charger_feed_watchdog_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc8989x_charger_info *info = container_of(dwork,
                                        struct sc8989x_charger_info,
                                        wdt_work);
    int ret = 0;

    ret = sc8989x_charger_feed_watchdog(info);
    if (ret)
        schedule_delayed_work(&info->wdt_work, HZ * 5);
    else
        schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if CONFIG_REGULATOR
static bool sc8989x_charger_check_otg_valid(struct sc8989x_charger_info *info)
{
    int ret = 0;
    u8 value = 0;
    bool status = false;

    ret = sc8989x_read(info, SC8989X_REG_03, &value);
    if (ret) {
        ZTE_CM_IC_ERROR("get charger otg valid status failed\n");
        return status;
    }

    if (value & REG03_OTG_MASK)
        status = true;
    else
        ZTE_CM_IC_ERROR("otg is not valid, REG_3 = 0x%x\n", value);

    return status;
}

static bool sc8989x_charger_check_otg_fault(struct sc8989x_charger_info *info)
{
    int ret = 0;
    u8 value = 0;
    bool status = true;

    ret = sc8989x_read(info, SC8989X_REG_0C, &value);
    if (ret) {
        ZTE_CM_IC_ERROR("get charger otg fault status failed\n");
        return status;
    }

    if (!(value & REG0C_OTG_FAULT))
        status = false;
    else
        ZTE_CM_IC_ERROR("boost fault occurs, REG_0C = 0x%x\n",
                value);

    return status;
}

static void sc8989x_charger_otg_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc8989x_charger_info *info = container_of(dwork,
                                        struct sc8989x_charger_info, otg_work);
    bool otg_valid = sc8989x_charger_check_otg_valid(info);
    bool otg_fault;
    int ret, retry = 0;

    if (otg_valid)
        goto out;

    do {
        otg_fault = sc8989x_charger_check_otg_fault(info);
        if (!otg_fault) {
            ret = sc8989x_update_bits(info, SC8989X_REG_03,
                                      REG03_CHG_MASK,
                                      REG03_CHG_DISABLE << REG03_CHG_SHIFT);

            ret = sc8989x_update_bits(info, SC8989X_REG_03,
                                      REG03_OTG_MASK,
                                      REG03_OTG_ENABLE << REG03_OTG_SHIFT);
            if (ret)
                ZTE_CM_IC_ERROR("restart charger otg failed\n");
        }

        otg_valid = sc8989x_charger_check_otg_valid(info);
    } while (!otg_valid && retry++ < SC8989X_OTG_RETRY_TIMES);

    if (retry >= SC8989X_OTG_RETRY_TIMES) {
        ZTE_CM_IC_ERROR("Restart OTG failed\n");
        return;
    }

out:
    schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sc8989x_charger_enable_otg(struct regulator_dev *dev)
{
    struct sc8989x_charger_info *info = rdev_get_drvdata(dev);
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
        ZTE_CM_IC_ERROR("failed to disable bc1.2 detect function.\n");
        mutex_unlock(&info->lock);
        return ret;
    }

    ret = sc8989x_update_bits(info, SC8989X_REG_03, REG03_CHG_MASK,
                              REG03_CHG_DISABLE << REG03_CHG_SHIFT);

    ret = sc8989x_update_bits(info, SC8989X_REG_03, REG03_OTG_MASK,
                              REG03_OTG_ENABLE << REG03_OTG_SHIFT);

    if (ret) {
        ZTE_CM_IC_ERROR("enable otg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                           BIT_DP_DM_BC_ENB, 0);
        mutex_unlock(&info->lock);
        return ret;
    }

    schedule_delayed_work(&info->wdt_work,
                          msecs_to_jiffies(SC8989X_WDT_VALID_MS));
    schedule_delayed_work(&info->otg_work,
                          msecs_to_jiffies(SC8989X_OTG_VALID_MS));

    mutex_unlock(&info->lock);

    ZTE_CM_IC_ERROR("enable otg exit\n");

    return 0;
}

static int sc8989x_charger_disable_otg(struct regulator_dev *dev)
{
    struct sc8989x_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

    if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

    ZTE_CM_IC_ERROR("disable otg into\n");

    info->otg_enable = false;

    mutex_lock(&info->lock);

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);
    ret = sc8989x_update_bits(info, SC8989X_REG_03, REG03_CHG_MASK,
                              REG03_CHG_ENABLE << REG03_CHG_SHIFT);
    ret = sc8989x_update_bits(info, SC8989X_REG_03,
                              REG03_OTG_MASK, REG03_OTG_DISABLE);
    if (ret) {
        ZTE_CM_IC_ERROR("disable otg failed\n");
        mutex_unlock(&info->lock);
        return ret;
    }

    /* Enable charger detection function to identify the charger type */
    ret = regmap_update_bits(info->pmic, info->charger_detect,
                              BIT_DP_DM_BC_ENB, 0);
    if (ret)
        ZTE_CM_IC_ERROR("enable BC1.2 failed\n");

    mutex_unlock(&info->lock);

    ZTE_CM_IC_ERROR("disable otg exit\n");

    return ret;
}

static int sc8989x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
    struct sc8989x_charger_info *info = rdev_get_drvdata(dev);

    if (!info) {
		ZTE_CM_IC_ERROR("NULL pointer!!!\n");
		return -EINVAL;
	}

    ZTE_CM_IC_INFO("otg_is_enabled %d!\n", info->otg_enable);

    return info->otg_enable;
}

static const struct regulator_ops sc8989x_charger_vbus_ops = {
    .enable = sc8989x_charger_enable_otg,
    .disable = sc8989x_charger_disable_otg,
    .is_enabled = sc8989x_charger_vbus_is_enabled,
};

static const struct regulator_desc sc8989x_charger_vbus_desc = {
    .name = "sc8989x_otg_vbus",
    .of_match = "sc8989x_otg_vbus",
    .type = REGULATOR_VOLTAGE,
    .owner = THIS_MODULE,
    .ops = &sc8989x_charger_vbus_ops,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int sc8989x_charger_register_vbus_regulator(struct sc8989x_charger_info *info)
{
    struct regulator_config cfg = { };
    struct regulator_dev *reg;
    int ret = 0;

    cfg.dev = info->dev;
    cfg.driver_data = info;
    reg = devm_regulator_register(info->dev,
                                  &sc8989x_charger_vbus_desc, &cfg);
    if (IS_ERR(reg)) {
        ret = PTR_ERR(reg);
        ZTE_CM_IC_ERROR("Can't register regulator:%d\n", ret);
    }

    return ret;
}

#else
static int sc8989x_charger_register_vbus_regulator(struct sc8989x_charger_info *info)
{
    return 0;
}
#endif

static int sc8989x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct sc8989x_charger_info *info =
		container_of(nb, struct sc8989x_charger_info, usb_notify);

	ZTE_CM_IC_INFO("sc8989x_charger_usb_change: %d\n", limit);
	info->limit = limit;

#ifdef CONFIG_VENDOR_SQC_CHARGER
	sqc_notify_daemon_changed(SQC_NOTIFY_USB,
					SQC_NOTIFY_USB_STATUS_CHANGED, !!limit);
#endif

	return NOTIFY_OK;
}

static void sc8989x_charger_dump_reg_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct sc8989x_charger_info *info = container_of(dwork,
                                        struct sc8989x_charger_info, dump_work);

    sc8989x_charger_dump_register(info);

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
		ZTE_CM_IC_INFO("sc8989x:psy name is NULL!!\n");
		goto failed_loop;
	}

	psy = zte_power_supply_get_by_name(name);
	if (!psy) {
		ZTE_CM_IC_INFO("sc8989x:get %s psy failed!!\n", name);
		goto failed_loop;
	}

	val.intval = data;

	rc = zte_power_supply_set_property(psy,
				psp, &val);
	if (rc < 0) {
		ZTE_CM_IC_INFO("sc8989x:Failed to set %s property:%d rc=%d\n", name, psp, rc);
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
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;

	sc8989x_charger_get_charge_status(info, charing_status);

	return 0;
}

static int sqc_mp_set_enable_chging(void *arg, unsigned int en)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;

	if (en) {
		
#ifndef ZTE_FEATURE_PV_AR
		sc8989x_charger_feed_watchdog(info);
#endif
		sc8989x_charger_start_charge(info);

		sc8989x_charger_set_vindpm(info, 4600);

		info->charging = true;

		sc8989x_charger_dump_register(info);
	} else {
		sc8989x_charger_stop_charge(info);

		sc8989x_charger_dump_register(info);
		info->charging = false;
	}

	ZTE_CM_IC_INFO("%d\n", en);

	return 0;
}

static int sqc_mp_get_enable_chging(void *arg, unsigned int *en)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_charge_status(info, en);

	ZTE_CM_IC_INFO("%d\n", *en);

	return ret;
}

static int sqc_mp_get_ichg(void *arg, u32 *ichg_ma)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_current(info, ichg_ma);

	ZTE_CM_IC_INFO("%d\n", *ichg_ma);

	return ret;
}

static int sqc_mp_set_ichg(void *arg, u32 mA)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_set_current(info, mA);

	ZTE_CM_IC_INFO("%d\n", mA);

	return ret;
}

static int sqc_mp_set_ieoc(void *arg, u32 mA)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_set_termina_cur(info, mA);

	ZTE_CM_IC_INFO("%d\n", mA);

	return ret;
}

static int sqc_mp_get_ieoc(void *arg, u32 *ieoc)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_termina_cur(info, ieoc);

	ZTE_CM_IC_INFO("%d\n", *ieoc);

	return ret;
}

static int sqc_mp_get_aicr(void *arg, u32 *aicr_ma)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("%d\n", *aicr_ma);

	return ret;
}

static int sqc_mp_set_aicr(void *arg, u32 aicr_ma)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_set_limit_current(info, aicr_ma);

	ZTE_CM_IC_INFO("%d\n", aicr_ma);

	return ret;
}

static int sqc_mp_get_cv(void *arg, u32 *cv_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("%d\n", *cv_mv);

	return ret;
}

static int sqc_mp_set_cv(void *arg, u32 cv_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_set_termina_vol(info, cv_mv);

	ZTE_CM_IC_INFO("%d\n", cv_mv);

	return ret;
}

static int sqc_mp_get_rechg_voltage(void *arg, u32 *rechg_volt_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("%d\n", *rechg_volt_mv);

	return ret;
}

static int sqc_mp_set_rechg_voltage(void *arg, u32 rechg_volt_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_set_recharging_vol(info, rechg_volt_mv);

	ZTE_CM_IC_INFO("%d\n", rechg_volt_mv);

	return ret;
}

static int sqc_mp_ovp_volt_get(void *arg, u32 *ac_ovp_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;

    *ac_ovp_mv = info->vbus_ovp_mv;

    ZTE_CM_IC_INFO("%dmV\n", *ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_ovp_volt_set(void *arg, u32 ac_ovp_mv)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;

    info->vbus_ovp_mv = ac_ovp_mv;

    ZTE_CM_IC_INFO("%dmV\n", ac_ovp_mv);
	
	return 0;
}

static int sqc_mp_get_vbat(void *arg, u32 *mV)
{
	union power_supply_propval batt_vol_uv;
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SC8989X_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SC8989X_FGU_NAME);
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

	fuel_gauge = power_supply_get_by_name(SC8989X_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SC8989X_FGU_NAME);
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

    ZTE_CM_IC_INFO("%mA\n", *mA);

	return 0;
}

static int sqc_mp_get_vbus(void *arg, u32 *mV)
{
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)arg;
	union power_supply_propval val = {};
	struct power_supply *fuel_gauge = NULL;
	int ret1 = 0;

	fuel_gauge = power_supply_get_by_name(SC8989X_FGU_NAME);
	if (!fuel_gauge) {
		ZTE_CM_IC_ERROR("get %s failed!\n", SC8989X_FGU_NAME);
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

    ZTE_CM_IC_INFO("adc:%dmV, vbus_ovp_mv:%dmV\n", *mV, info->vbus_ovp_mv);

    if (*mV > info->vbus_ovp_mv) {
        if (info->ovp_enable != true) {
            info->ovp_enable = true;
            ZTE_CM_IC_INFO("vbus(%d) more that ovp(%d), enable hiz\n", *mV, info->vbus_ovp_mv);
            sc8989x_charger_set_hiz(info, true);
        }
    } else {
        if (info->ovp_enable != false) {
            info->ovp_enable = false;
            ZTE_CM_IC_INFO("vbus(%d) less that ovp(%d), disable hiz\n", *mV, info->vbus_ovp_mv);
            sc8989x_charger_set_hiz(info, false);
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
	struct sc8989x_charger_info *chg_dev = (struct sc8989x_charger_info *)arg;

	return 0;
}

static int sqc_enable_powerpath_get(void *arg, unsigned int *enabled)
{
	struct sc8989x_charger_info *chg_dev = (struct sc8989x_charger_info *)arg;

	return 0;
}
*/
static int sqc_enable_hiz_set(void *arg, unsigned int enable)
{
	struct sc8989x_charger_info *chg_dev = (struct sc8989x_charger_info *)arg;

	sc8989x_charger_set_hiz(chg_dev, !!enable);

	return 0;
}

static int sqc_enable_hiz_get(void *arg, unsigned int *enable)
{
	struct sc8989x_charger_info *chg_dev = (struct sc8989x_charger_info *)arg;
	int ret = 0;

	ret = sc8989x_charger_get_hiz(chg_dev, enable);

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
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)sqc_bc1d2_proto_node.arg;

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
	struct sc8989x_charger_info *info = (struct sc8989x_charger_info *)sqc_bc1d2_proto_node.arg;

	sscanf(val, "%d", &sleep_mode_enable);

	ZTE_CM_IC_INFO("sleep_mode_enable = %d\n", sleep_mode_enable);

	if (sleep_mode_enable) {
		if (sqc_chg_type != SQC_SLEEP_MODE_TYPE) {
			ZTE_CM_IC_INFO("sleep on status");

			/*disabel sqc-daemon*/
			sqc_chg_type = SQC_SLEEP_MODE_TYPE;
			sqc_notify_daemon_changed(SQC_NOTIFY_USB, SQC_NOTIFY_USB_STATUS_CHANGED, 1);

            ret = sc8989x_update_bits(info, SC8989X_REG_07, REG07_TWD_MASK,
                              REG07_TWD_DISABLE << REG07_TWD_SHIFT);
            if (ret) {
                ZTE_CM_IC_ERROR("Failed to disable watchdog\n");
            }
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

static int sc8989x_charger_probe(struct i2c_client *client,
                                 const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct device *dev = &client->dev;
    struct power_supply_config charger_cfg = { };
    struct sc8989x_charger_info *info;
    struct device_node *regmap_np;
    struct platform_device *regmap_pdev;
    struct sqc_pmic_chg_ops *sqc_ops = NULL;
    int ret = 0;

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        ZTE_CM_IC_ERROR("No support for SMBUS_BYTE_DATA\n");
        return -ENODEV;
    }

    ZTE_CM_IC_INFO("(%s): initializing...\n", SC8989X_DRV_VERSION);

    info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;
    info->client = client;
    info->dev = dev;

    alarm_init(&info->wdg_timer, ALARM_BOOTTIME, NULL);

    mutex_init(&info->lock);
    mutex_init(&info->input_limit_cur_lock);

    INIT_DELAYED_WORK(&info->otg_work, sc8989x_charger_otg_work);
    INIT_DELAYED_WORK(&info->wdt_work, sc8989x_charger_feed_watchdog_work);
    INIT_DELAYED_WORK(&info->dump_work, sc8989x_charger_dump_reg_work);

    i2c_set_clientdata(client, info);

    info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
    if (IS_ERR(info->usb_phy)) {
        ZTE_CM_IC_ERROR("failed to find USB phy\n");
        goto err_mutex_lock;
    }

    ret = sc8989x_charger_is_fgu_present(info);
    if (ret) {
        ZTE_CM_IC_ERROR("sc27xx_fgu not ready.\n");
        return -EPROBE_DEFER;
    }

    ret = sc8989x_charger_register_vbus_regulator(info);
    if (ret) {
        ZTE_CM_IC_ERROR("failed to register vbus regulator.\n");
        return ret;
    }

    regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
    if (!regmap_np)
        regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

    if (regmap_np) {
        if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
            info->charger_pd_mask = SC8989X_DISABLE_PIN_MASK_2721;
        else
            info->charger_pd_mask = SC8989X_DISABLE_PIN_MASK;
    } else {
        ZTE_CM_IC_ERROR("unable to get syscon node\n");
        return -ENODEV;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 1,
                                     &info->charger_detect);
    if (ret) {
        ZTE_CM_IC_ERROR("failed to get charger_detect\n");
        return -EINVAL;
    }

    ret = of_property_read_u32_index(regmap_np, "reg", 2,
                                     &info->charger_pd);
    if (ret) {
        ZTE_CM_IC_ERROR("failed to get charger_pd reg\n");
        return ret;
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

	info->usb_notify.notifier_call = sc8989x_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		ZTE_CM_IC_ERROR("failed to register notifier:%d\n", ret);
		goto err_mutex_lock;
	}

    charger_cfg.drv_data = info;
    charger_cfg.of_node = dev->of_node;
    info->psy_usb = devm_power_supply_register(dev,
                    &sc8989x_charger_desc,
                    &charger_cfg);

    if (IS_ERR(info->psy_usb)) {
        ZTE_CM_IC_ERROR("failed to register power supply\n");
        ret = PTR_ERR(info->psy_usb);
        goto err_usb_notifier;
    }

    ret = sc8989x_charger_hw_init(info);
    if (ret) {
        ZTE_CM_IC_ERROR("failed to sc8989x_charger_hw_init\n");
        goto err_usb_notifier;
    }

    sc8989x_charger_stop_charge(info);

    device_init_wakeup(info->dev, true);

    ret = sc8989x_register_sysfs(info);
    if (ret) {
        ZTE_CM_IC_ERROR("register sysfs fail, ret = %d\n", ret);
        goto err_sysfs;
    }

    ret = sc8989x_update_bits(info, SC8989X_REG_07, REG07_TWD_MASK,
                              REG07_TWD_40S << REG07_TWD_SHIFT);
    if (ret) {
        ZTE_CM_IC_ERROR("Failed to enable watchdog\n");
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

    return 0;

err_sysfs:
    sysfs_remove_group(&info->dev->kobj, &info->sysfs->attr_g);
err_usb_notifier:
    usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_mutex_lock:
    mutex_destroy(&info->lock);
    return ret;
}

static void sc8989x_charger_shutdown(struct i2c_client *client)
{
    struct sc8989x_charger_info *info = i2c_get_clientdata(client);
    int ret = 0;

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->dump_work);
    if (info->otg_enable) {
        cancel_delayed_work_sync(&info->otg_work);
        ret = sc8989x_update_bits(info, SC8989X_REG_03,
                                  REG03_OTG_MASK,
                                  0);
        if (ret)
            ZTE_CM_IC_ERROR("disable otg failed ret = %d\n", ret);

        /* Enable charger detection function to identify the charger type */
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                                 BIT_DP_DM_BC_ENB, 0);
        if (ret)
            ZTE_CM_IC_ERROR("enable charger detection function failed ret = %d\n", ret);
    }
}

static int sc8989x_charger_remove(struct i2c_client *client)
{
    struct sc8989x_charger_info *info = i2c_get_clientdata(client);

    usb_unregister_notifier(info->usb_phy, &info->usb_notify);

    cancel_delayed_work_sync(&info->dump_work);
    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc8989x_charger_suspend(struct device *dev)
{
    struct sc8989x_charger_info *info = dev_get_drvdata(dev);
    ktime_t now, add;
    unsigned int wakeup_ms = SC8989X_WDG_TIMER_MS;

    if (info->otg_enable)
        /* feed watchdog first before suspend */
        sc8989x_charger_feed_watchdog(info);

    if (!info->otg_enable)
        return 0;

    cancel_delayed_work_sync(&info->dump_work);
    cancel_delayed_work_sync(&info->wdt_work);

    now = ktime_get_boottime();
    add = ktime_set(wakeup_ms / MSEC_PER_SEC,
                    (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
    alarm_start(&info->wdg_timer, ktime_add(now, add));

    return 0;
}

static int sc8989x_charger_resume(struct device *dev)
{
    struct sc8989x_charger_info *info = dev_get_drvdata(dev);

    if (info->otg_enable)
        /* feed watchdog first before suspend */
        sc8989x_charger_feed_watchdog(info);

    if (!info->otg_enable)
        return 0;

    alarm_cancel(&info->wdg_timer);

    schedule_delayed_work(&info->wdt_work, HZ * 15);
    schedule_delayed_work(&info->dump_work, HZ * 15);

    return 0;
}
#endif

static const struct dev_pm_ops sc8989x_charger_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(sc8989x_charger_suspend,
                            sc8989x_charger_resume)
};

static const struct i2c_device_id sc8989x_i2c_id[] = {
    {"sc8989x_chg", 0},
    {}
};

static const struct of_device_id sc8989x_charger_of_match[] = {
    { .compatible = "sc,sc8989x_chg", },
    { }
};

MODULE_DEVICE_TABLE(of, sc8989x_charger_of_match);

static struct i2c_driver sc8989x_charger_driver = {
    .driver = {
        .name = "sc8989x_chg",
        .of_match_table = sc8989x_charger_of_match,
        .pm = &sc8989x_charger_pm_ops,
    },
    .probe = sc8989x_charger_probe,
    .shutdown = sc8989x_charger_shutdown,
    .remove = sc8989x_charger_remove,
    .id_table = sc8989x_i2c_id,
};

module_i2c_driver(sc8989x_charger_driver);

MODULE_AUTHOR("Aiden Yu<Aiden-yu@southchip.com>");
MODULE_DESCRIPTION("SC8989X Charger Driver");
MODULE_LICENSE("GPL v2");

