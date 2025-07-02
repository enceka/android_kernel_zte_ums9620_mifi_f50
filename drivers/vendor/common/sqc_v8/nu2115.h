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

#ifndef _NU2115_H_
#define _NU2115_H_

#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/workqueue.h>

#ifndef BIT
#define BIT(x)    (1 << (x))
#endif

enum {
	NU2115_STDALONE,
	NU2115_MASTER,
	NU2115_SLAVE,
};


/*************************struct define area***************************/
struct nu2115_device {
	struct i2c_client *client;
	struct device *dev;
	struct work_struct irq_work;
	int gpio_int;
	int irq_int;
	int irq_active;
	int chip_already_init;
	int device_id;
	long hw_mode;
	u32 stat;
	int chip_enabled;
	int chip_is_enable;
	char *name;
	struct extcon_dev *edev;
	struct notifier_block edv_nb;
	struct delayed_work otg_work;
};

/*************************marco define area***************************/
#define NU2115_INIT_FINISH                (1)
#define NU2115_NOT_INIT                   (0)
#define NU2115_ENABLE_INT_NOTIFY          (1)
#define NU2115_DISABLE_INT_NOTIFY         (0)

#define NU2115_NOT_USED                   (0)
#define NU2115_USED                       (1)
#define NU2115_DEVICE_ID_GET_FAIL         (-1)
#define NU2115_REG_RESET                  (1)
#define NU2115_SWITCHCAP_DISABLE          (0)

#define NU2115_LENTH_OF_BYTE              (8)
#define NU2115_TDIE_SCALE                 (2)

#define NU2115_ADC_DISABLE                (0)
#define NU2115_ADC_ENABLE                 (1)

#define NU2115_ALM_DISABLE                (0)
#define NU2115_ALM_ENABLE                 (1)

#define NU2115_AC_OVP_THRESHOLD_INIT      (12000)  /* 12v */
#define NU2115_VBUS_OVP_THRESHOLD_INIT    (12000)  /* 12v */
#define NU2115_IBUS_OCP_THRESHOLD_INIT    (4750)   /* 4.75a */
#define NU2115_VBAT_OVP_THRESHOLD_INIT    (5000)   /* 5v */
#define NU2115_IBAT_OCP_THRESHOLD_INIT    (10000)  /* 10a */
#define NU2115_IBAT_OCP_ALARM_THRESHOLD_INIT    (8000)  /* 10a */

//#define NU2115_RESISTORS_10KOHM           (10)     /* for Rhi and Rlo */
//#define NU2115_RESISTORS_100KOHM          (100)    /* for Rhi and Rlo */
//#define NU2115_RESISTORS_KILO             (1000)   /* kilo */

/* BAT_OVP reg=0x00
 * default=4.35v [b10 0010], BAT_OVP=3.5v + bat_ovp[5:0]*25mv
 */
#define NU2115_BAT_OVP_REG                0x00
#define NU2115_BAT_OVP_MASK               (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define NU2115_BAT_OVP_SHIFT              (0)
#define NU2115_BAT_OVP_MAX_5075MV         (5075)   /* 5.075v */
#define NU2115_BAT_OVP_BASE_3500MV        (3500)   /* 3.5v */
#define NU2115_BAT_OVP_DEFAULT            (4350)   /* 4.35v */
#define NU2115_BAT_OVP_STEP               (156)     /* 25mv */

#define NU2115_BAT_OVP_DIS_MASK           (BIT(7))
#define NU2115_BAT_OVP_DIS_SHIFT          (7)

/* BAT_OVP_ALM reg=0x01
 * default=4.20v [b01 1100], BAT_OVP_ALM=3.5v + bat_ovp_alm[5:0]*25mv
 */
#define NU2115_BAT_OVP_ALM_REG            0x01
#define NU2115_BAT_OVP_ALM_MASK           (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define NU2115_BAT_OVP_ALM_SHIFT          (0)
#define NU2115_BAT_OVP_ALM_MAX            (5075)   /* 5.075v */
#define NU2115_BAT_OVP_ALM_BASE           (3500)   /* 3.5v */
#define NU2115_BAT_OVP_ALM_DEFAULT        (4200)   /* 4.2v */
#define NU2115_BAT_OVP_ALM_STEP           (15)     /* 25mv */

#define NU2115_BAT_OVP_ALM_DIS_MASK       (BIT(7))
#define NU2115_BAT_OVP_ALM_DIS_SHIFT      (7)

/* BAT_OCP reg=0x02
 * default=8.1a [b011 1101], BAT_OCP=2a + bat_ocp[6:0]*100ma
 */
#define NU2115_BAT_OCP_REG                0x02
#define NU2115_BAT_OCP_MASK               (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define NU2115_BAT_OCP_SHIFT              (0)
#define NU2115_BAT_OCP_MAX_14700MA        (14700)  /* 14.7a */
#define NU2115_BAT_OCP_BASE_2000MA        (2000)   /* 2.0a */
#define NU2115_BAT_OCP_DEFAULT            (8100)   /* 8.1a */
#define NU2115_BAT_OCP_STEP               (100)    /* 100ma */

#define NU2115_BAT_OCP_DIS_MASK           (BIT(7))
#define NU2115_BAT_OCP_DIS_SHIFT          (7)

/* BAT_OCP_ALM reg=0x03
 * default=8.0a [b011 1100], BAT_OCP_ALM=2a + bat_ocp_alm[6:0]*100ma
 */
#define NU2115_BAT_OCP_ALM_REG            0x03
#define NU2115_BAT_OCP_ALM_MASK           (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5) | BIT(6))
#define NU2115_BAT_OCP_ALM_SHIFT          (0)
#define NU2115_BAT_OCP_ALM_MAX            (14700)  /* 14.7a */
#define NU2115_BAT_OCP_ALM_BASE           (2000)   /* 2.0a */
#define NU2115_BAT_OCP_ALM_DEFAULT        (8000)   /* 8.0a */
#define NU2115_BAT_OCP_ALM_STEP           (100)    /* 100ma */

#define NU2115_BAT_OCP_ALM_DIS_MASK       (BIT(7))
#define NU2115_BAT_OCP_ALM_DIS_SHIFT      (7)

/* BAT_UCP_ALM reg=0x04
 * default=2.0a [b010 1000], BAT_UCP_ALM=bat_ucp_alm[6:0]*50ma
 */
#define NU2115_BAT_UCP_ALM_REG            0x04
#define NU2115_BAT_UCP_ALM_MASK           (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5))
#define NU2115_BAT_UCP_ALM_SHIFT          (0)
#define NU2115_BAT_UCP_ALM_MAX            (6350)   /* 6.35a */
#define NU2115_BAT_UCP_ALM_BASE           (0)      /* 0a */
#define NU2115_BAT_UCP_ALM_DEFAULT        (2000)   /* 2a */
#define NU2115_BAT_UCP_ALM_STEP           (50)     /* 50ma */

#define NU2115_BAT_UCP_ALM_DIS_MASK       (BIT(7))
#define NU2115_BAT_UCP_ALM_DIS_SHIFT      (7)

/* AC_PROTECTION reg=0x05
 * default=14.0v [b111], AC_OVP=11v + ac_ovp[2:0]*1v
 */
#define NU2115_AC_OVP_REG                 0x05
#define NU2115_AC_OVP_MASK                (BIT(0) | BIT(1) | BIT(2))
#define NU2115_AC_OVP_SHIFT               (0)
#define NU2115_AC_OVP_MAX_13000MV         (13000)  /* 13v */
#define NU2115_AC_OVP_BASE_10000MV        (10000)  /* 10v */
#define NU2115_AC_OVP_DEFAULT             (6500)  /* 6.5v */
#define NU2115_AC_OVP_STEP                (500)   /* 0.5v */

#define NU2115_AC_OVP_STAT_MASK           (BIT(7))
#define NU2115_AC_OVP_STAT_SHIFT          (7)
#define NU2115_AC_OVP_FLAG_MASK           (BIT(6))
#define NU2115_AC_OVP_FLAG_SHIFT          (6)
#define NU2115_AC_OVP_MASK_MASK           (BIT(5))
#define NU2115_AC_OVP_MASK_SHIFT          (5)
#define NU2115_AC_PD_EN_MASK           (BIT(4))
#define NU2115_AC_PD_EN_SHIFT          (4)


/* BUS_OVP reg=0x07
 * default=8.9v [b011 1010], BUS_OVP=6v + bus_ovp[6:0]*50mv
 */
#define NU2115_BUS_OVP_REG                0x07
#define NU2115_VBUS_PD_EN_MASK            (BIT(7))
#define NU2115_VBUS_PD_EN_SHIFT           (7)
#define NU2115_VBUS_OVP_DIS_MASK            (BIT(6))
#define NU2115_VBUS_OVP_DIS_SHIFT           (6)
#define NU2115_BUS_OVP_MASK               (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5))
#define NU2115_BUS_OVP_SHIFT              (0)
#define NU2115_BUS_OVP_MAX_12300MV        (12300)  /* 12.3v */
#define NU2115_BUS_OVP_BASE_6000MV        (6000)   /* 6v */
#define NU2115_BUS_OVP_DEFAULT            (9000)   /* 9v */
#define NU2115_BUS_OVP_STEP               (100)     /* 100mv */

/* BUS_OVP_ALM reg=0x08
 * default=8.8v [b011 1000], BUS_OVP_ALM=6v + bus_ovp_alm[6:0]*50mv
 */
#define NU2115_BUS_OVP_ALM_REG            0x08
#define NU2115_BUS_OVP_ALM_MASK           (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4) | BIT(5))
#define NU2115_BUS_OVP_ALM_SHIFT          (0)
#define NU2115_BUS_OVP_ALM_MAX            (12300)  /* 12.3v */
#define NU2115_BUS_OVP_ALM_BASE           (6000)   /* 6v */
#define NU2115_BUS_OVP_ALM_DEFAULT        (8900)   /* 8.9v */
#define NU2115_BUS_OVP_ALM_STEP           (100)     /* 100mv */

#define NU2115_BUS_OVP_ALM_DIS_MASK       (BIT(7))
#define NU2115_BUS_OVP_ALM_DIS_SHIFT      (7)
#define NU2115_BUS_RCP_DIS_MASK       (BIT(6))
#define NU2115_BUS_RCP_DIS_SHIFT      (6)

/* BUS_OCP_UCP reg=0x09
 * default=4.25a [b1101], BUS_OCP_UCP=1a + bus_ocp[3:0]*250ma
 */
#define NU2115_BUS_OCP_UCP_REG            0x09
#define NU2115_BUS_OCP_MASK               (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define NU2115_BUS_OCP_SHIFT              (0)
#define NU2115_BUS_OCP_MAX_6250MA         (6250)  /* 6.25a */
#define NU2115_BUS_OCP_BASE_2500MA        (2500)  /* 2.5a */
#define NU2115_BUS_OCP_DEFAULT            (2750)  /* 2.75a */
#define NU2115_BUS_OCP_STEP               (250)   /* 250ma */

#define NU2115_BUS_OCP_DIS_MASK           (BIT(7))
#define NU2115_BUS_OCP_DIS_SHIFT          (7)
#define NU2115_IBUS_UCP_RISE_FLAG_MASK    (BIT(6))
#define NU2115_IBUS_UCP_RISE_FLAG_SHIFT   (6)
#define NU2115_IBUS_UCP_RISE_MASK_MASK    (BIT(5))
#define NU2115_IBUS_UCP_RISE_MASK_SHIFT   (5)
#define NU2115_IBUS_UCP_DIS_MASK    (BIT(4))
#define NU2115_IBUS_UCP_DIS_SHIFT   (4)

/* BUS_OCP_ALM reg=0x0A
 * default=4.0a [b101 0000], BUS_OCP_ALM=bus_ocp_alm[6:0]*50ma
 */
#define NU2115_BUS_OCP_ALM_REG            0x0A
#define NU2115_BUS_OCP_ALM_MASK           (BIT(0) | BIT(1) | BIT(2) | \
	BIT(3) | BIT(4))
#define NU2115_BUS_OCP_ALM_SHIFT          (0)
#define NU2115_BUS_OCP_ALM_MAX            (6375)  /* 6.375a */
#define NU2115_BUS_OCP_ALM_BASE           (2500)     /* 2.5a */
#define NU2115_BUS_OCP_ALM_DEFAULT        (2500)  /* 2.5a */
#define NU2115_BUS_OCP_ALM_STEP           (125)    /* 125ma */

#define NU2115_BUS_OCP_ALM_DIS_MASK       (BIT(7))
#define NU2115_BUS_OCP_ALM_DIS_SHIFT      (7)
#define NU2115_IBUS_UCP_FALL_FLAG_MASK    (BIT(6))
#define NU2115_IBUS_UCP_FALL_FLAG_SHIFT   (6)
#define NU2115_IBUS_UCP_FALL_MASK_MASK    (BIT(5))
#define NU2115_IBUS_UCP_FALL_MASK_SHIFT   (5)

/* VOUT_OVP reg=0x0b */
#define NU2115_VOUT_OVP_REG        0x0B
#define NU2115_VOUT_OVP_DIS_MASK       (BIT(7))
#define NU2115_VOUT_OVP_DIS_SHIFT      (7)
#define NU2115_VOUT_OVP_STAT_MASK            (BIT(2))
#define NU2115_VOUT_OVP_STAT_SHIFT           (2)
#define NU2115_VOUT_OVP_FLAG_MASK            (BIT(1))
#define NU2115_VOUT_OVP_FLAG_SHIFT           (1)
#define NU2115_VOUT_OVP_MASK_MASK            (BIT(0))
#define NU2115_VOUT_OVP_MASK_SHIFT           (0)

/* CONVERTER_STATE reg=0x0c */
#define NU2115_CONVERTER_STATE_REG        0x0C

#define NU2115_TSHUT_FLAG_MASK            (BIT(7))
#define NU2115_TSHUT_FLAG_SHIFT           (7)
#define NU2115_TSHUT_STAT_MASK            (BIT(6))
#define NU2115_TSHUT_STAT_SHIFT           (6)
#define NU2115_VBUS_ERRORLO_FLAG_MASK     (BIT(5))
#define NU2115_VBUS_ERRORLO_FLAG_SHIFT    (5)
#define NU2115_VBUS_ERRORHI_FLAG_MASK     (BIT(4))
#define NU2115_VBUS_ERRORHI_FLAG_SHIFT    (4)
#define NU2115_SS_TIMEOUT_FLAG_MASK       (BIT(3))
#define NU2115_SS_TIMEOUT_FLAG_SHIFT      (3)
#define NU2115_CONV_STAT_FLAG_MASK       (BIT(2))
#define NU2115_CONV_STAT_FLAG_SHIFT      (2)
#define NU2115_PIN_DIAG_FAIL_FLAG_MASK    (BIT(0))
#define NU2115_PIN_DIAG_FAIL_FLAG_SHIFT   (0)

/* CONTROL reg=0x0d */
#define NU2115_CONTROL_REG                0x0D
/* 0x27=disable watchdog, switching freq 500khz */
#define NU2115_CONTROL_REG_INIT           (0x27)

#define NU2115_REG_RST_MASK               (BIT(7))
#define NU2115_REG_RST_SHIFT              (7)
#define NU2115_FSW_SET_MASK               (BIT(4) | BIT(5) | BIT(6))
#define NU2115_FSW_SET_SHIFT              (4)
#define NU2115_WATCHDOG_TIMEOUT_MASK      (BIT(3))
#define NU2115_WATCHDOG_TIMEOUT_SHIFT     (3)
#define NU2115_WATCHDOG_DIS_MASK          (BIT(2))
#define NU2115_WATCHDOG_DIS_SHIFT         (2)
#define NU2115_WATCHDOG_CONFIG_MASK       (BIT(0) | BIT(1))
#define NU2115_WATCHDOG_CONFIG_SHIFT      (0)

#define NU2115_REG_RST_ENABLE             (1)
/*
#define NU2115_SW_FREQ_450KHZ             (450)
#define NU2115_SW_FREQ_500KHZ             (500)
#define NU2115_SW_FREQ_550KHZ             (550)
#define NU2115_SW_FREQ_675KHZ             (675)
#define NU2115_SW_FREQ_750KHZ             (750)
#define NU2115_SW_FREQ_825KHZ             (825)

#define NU2115_FSW_SET_SW_FREQ_187P5KHZ   (0x0)
#define NU2115_FSW_SET_SW_FREQ_250KHZ     (0x1)
#define NU2115_FSW_SET_SW_FREQ_300KHZ     (0x2)
#define NU2115_FSW_SET_SW_FREQ_375KHZ     (0x3)
#define NU2115_FSW_SET_SW_FREQ_500KHZ     (0x4)
#define NU2115_FSW_SET_SW_FREQ_750KHZ     (0x5)

#define NU2115_WTD_CONFIG_TIMING_500MS    (0x0)
#define NU2115_WTD_CONFIG_TIMING_1000MS   (0x1)
#define NU2115_WTD_CONFIG_TIMING_5000MS   (0x2)
#define NU2115_WTD_CONFIG_TIMING_30000MS  (0x3)
*/
/* CHRG_CTRL reg=0x0c */
#define NU2115_CHRG_CTL_REG               0x0E
#define NU2115_CHRG_CTL_REG_INIT          (0x06)

#define NU2115_CHARGE_EN_MASK             (BIT(7))
#define NU2115_CHARGE_EN_SHIFT            (7)
#define NU2115_CHG_MODE_MASK                    (BIT(5) | BIT(6))
#define NU2115_CHG_MODE_SHIFT                   (5)
#define NU2115_FREQ_SHIFT_MASK            (BIT(3) | BIT(4))
#define NU2115_FREQ_SHIFT_SHIFT           (3)
#define NU2115_TSBAT_DIS_MASK             (BIT(2))
#define NU2115_TSBAT_DIS_SHIFT            (2)
#define NU2115_TSBUS_DIS_MASK             (BIT(1))
#define NU2115_TSBUS_DIS_SHIFT            (1)
#define NU2115_TDIE_DIS_MASK              (BIT(0))
#define NU2115_TDIE_DIS_SHIFT             (0)

#define NU2115_SW_FREQ_SHIFT_NORMAL       (0)
#define NU2115_SW_FREQ_SHIFT_P_P10        (1)  /* +%10 */
#define NU2115_SW_FREQ_SHIFT_M_P10        (2)  /* -%10 */

/* INT_STAT reg=0x0f */
#define NU2115_INT_STAT_REG               0x0F

#define NU2115_BAT_OVP_ALM_STAT_MASK      (BIT(7))
#define NU2115_BAT_OVP_ALM_STAT_SHIFT     (7)
#define NU2115_BAT_OCP_ALM_STAT_MASK      (BIT(6))
#define NU2115_BAT_OCP_ALM_STAT_SHIFT     (6)
#define NU2115_BUS_OVP_ALM_STAT_MASK      (BIT(5))
#define NU2115_BUS_OVP_ALM_STAT_SHIFT     (5)
#define NU2115_BUS_OCP_ALM_STAT_MASK      (BIT(4))
#define NU2115_BUS_OCP_ALM_STAT_SHIFT     (4)
#define NU2115_BAT_UCP_ALM_STAT_MASK      (BIT(3))
#define NU2115_BAT_UCP_ALM_STAT_SHIFT     (3)
#define NU2115_VBAT_INSERT_STAT_MASK      (BIT(1))
#define NU2115_VBAT_INSERT_STAT_SHIFT     (1)
#define NU2115_ADC_DONE_STAT_MASK         (BIT(0))
#define NU2115_ADC_DONE_STAT_SHIFT        (0)

/* INT_FLAG reg=0x10 */
#define NU2115_INT_FLAG_REG               0x10

#define NU2115_BAT_OVP_ALM_FLAG_MASK      (BIT(7))
#define NU2115_BAT_OVP_ALM_FLAG_SHIFT     (7)
#define NU2115_BAT_OCP_ALM_FLAG_MASK      (BIT(6))
#define NU2115_BAT_OCP_ALM_FLAG_SHIFT     (6)
#define NU2115_BUS_OVP_ALM_FLAG_MASK      (BIT(5))
#define NU2115_BUS_OVP_ALM_FLAG_SHIFT     (5)
#define NU2115_BUS_OCP_ALM_FLAG_MASK      (BIT(4))
#define NU2115_BUS_OCP_ALM_FLAG_SHIFT     (4)
#define NU2115_BAT_UCP_ALM_FLAG_MASK      (BIT(3))
#define NU2115_BAT_UCP_ALM_FLAG_SHIFT     (3)
#define NU2115_VBAT_INSERT_FLAG_MASK      (BIT(1))
#define NU2115_VBAT_INSERT_FLAG_SHIFT     (1)
#define NU2115_ADC_DONE_FLAG_MASK         (BIT(0))
#define NU2115_ADC_DONE_FLAG_SHIFT        (0)

/* INT_MASK reg=0x11 */
#define NU2115_INT_MASK_REG               0x11
#define NU2115_INT_MASK_REG_INIT          (0xB3)

#define NU2115_BAT_OVP_ALM_MASK_MASK      (BIT(7))
#define NU2115_BAT_OVP_ALM_MASK_SHIFT     (7)
#define NU2115_BAT_OCP_ALM_MASK_MASK      (BIT(6))
#define NU2115_BAT_OCP_ALM_MASK_SHIFT     (6)
#define NU2115_BUS_OVP_ALM_MASK_MASK      (BIT(5))
#define NU2115_BUS_OVP_ALM_MASK_SHIFT     (5)
#define NU2115_BUS_OCP_ALM_MASK_MASK      (BIT(4))
#define NU2115_BUS_OCP_ALM_MASK_SHIFT     (4)
#define NU2115_BAT_UCP_ALM_MASK_MASK      (BIT(3))
#define NU2115_BAT_UCP_ALM_MASK_SHIFT     (3)
#define NU2115_VBAT_INSERT_MASK_MASK      (BIT(1))
#define NU2115_VBAT_INSERT_MASK_SHIFT     (1)
#define NU2115_ADC_DONE_MASK_MASK         (BIT(0))
#define NU2115_ADC_DONE_MASK_SHIFT        (0)

/* FLT_STAT reg=0x12 */
#define NU2115_FLT_STAT_REG               0x12

#define NU2115_BAT_OVP_FLT_STAT_MASK      (BIT(7))
#define NU2115_BAT_OVP_FLT_STAT_SHIFT     (7)
#define NU2115_BAT_OCP_FLT_STAT_MASK      (BIT(6))
#define NU2115_BAT_OCP_FLT_STAT_SHIFT     (6)
#define NU2115_BUS_OVP_FLT_STAT_MASK      (BIT(5))
#define NU2115_BUS_OVP_FLT_STAT_SHIFT     (5)
#define NU2115_BUS_OCP_FLT_STAT_MASK      (BIT(4))
#define NU2115_BUS_OCP_FLT_STAT_SHIFT     (4)
#define NU2115_BUS_RCP_FLT_STAT_MASK  (BIT(3))
#define NU2115_BUS_RCP_FLT_STAT_SHIFT (3)
#define NU2115_TS_ALM_STAT_MASK        (BIT(2))
#define NU2115_TS_ALM_STAT_SHIFT       (2)
#define NU2115_TS_FLT_STAT_MASK        (BIT(1))
#define NU2115_TS_FLT_STAT_SHIFT       (1)
#define NU2115_TDIE_ALM_STAT_MASK         (BIT(0))
#define NU2115_TDIE_ALM_STAT_SHIFT        (0)

/* FLT_FLAG reg=0x13 */
#define NU2115_FLT_FLAG_REG               0x13

#define NU2115_BAT_OVP_FLT_FLAG_MASK      (BIT(7))
#define NU2115_BAT_OVP_FLT_FLAG_SHIFT     (7)
#define NU2115_BAT_OCP_FLT_FLAG_MASK      (BIT(6))
#define NU2115_BAT_OCP_FLT_FLAG_SHIFT     (6)
#define NU2115_BUS_OVP_FLT_FLAG_MASK      (BIT(5))
#define NU2115_BUS_OVP_FLT_FLAG_SHIFT     (5)
#define NU2115_BUS_OCP_FLT_FLAG_MASK      (BIT(4))
#define NU2115_BUS_OCP_FLT_FLAG_SHIFT     (4)
#define NU2115_BUS_RCP_FLT_FLAG_MASK  (BIT(3))
#define NU2115_BUS_RCP_FLT_FLAG_SHIFT (3)
#define NU2115_TS_ALM_FLAG_MASK        (BIT(2))
#define NU2115_TS_ALM_FLAG_SHIFT       (2)
#define NU2115_TS_FLT_FLAG_MASK        (BIT(1))
#define NU2115_TS_FLT_FLAG_SHIFT       (1)
#define NU2115_TDIE_ALM_FLAG_MASK         (BIT(0))
#define NU2115_TDIE_ALM_FLAG_SHIFT        (0)

/* FLT_MASK reg=0x14 */
#define NU2115_FLT_MASK_REG               0x14
#define NU2115_FLT_MASK_REG_INIT          (0x00)

#define NU2115_BAT_OVP_FLT_MASK_MASK      (BIT(7))
#define NU2115_BAT_OVP_FLT_MASK_SHIFT     (7)
#define NU2115_BAT_OCP_FLT_MASK_MASK      (BIT(6))
#define NU2115_BAT_OCP_FLT_MASK_SHIFT     (6)
#define NU2115_BUS_OVP_FLT_MASK_MASK      (BIT(5))
#define NU2115_BUS_OVP_FLT_MASK_SHIFT     (5)
#define NU2115_BUS_OCP_FLT_MASK_MASK      (BIT(4))
#define NU2115_BUS_OCP_FLT_MASK_SHIFT     (4)
#define NU2115_BUS_RCP_FLT_MASK_MASK  (BIT(3))
#define NU2115_BUS_RCP_FLT_MASK_SHIFT (3)
#define NU2115_TS_ALM_MASK_MASK        (BIT(2))
#define NU2115_TS_ALM_MASK_SHIFT       (2)
#define NU2115_TS_FLT_MASK_MASK        (BIT(1))
#define NU2115_TS_FLT_MASK_SHIFT       (1)
#define NU2115_TDIE_ALM_MASK_MASK         (BIT(0))
#define NU2115_TDIE_ALM_MASK_SHIFT        (0)

/* ADC_CTRL reg=0x15 */
#define NU2115_ADC_CTRL_REG               0x15
#define NU2115_ADC_CTRL_REG_INIT          (0xA0)
#define NU2115_ADC_CTRL_REG_EXIT          (0x00)

#define NU2115_ADC_CTRL_EN_MASK           (BIT(7))
#define NU2115_ADC_CTRL_EN_SHIFT          (7)
#define NU2115_ADC_RATE_MASK              (BIT(6))
#define NU2115_ADC_RATE_SHIFT             (6)
#define NU2115_ADC_AVG_MASK               (BIT(5))
#define NU2115_ADC_AVG_SHIFT              (5)
#define NU2115_IBUS_ADC_DIS_MASK          (BIT(1))
#define NU2115_IBUS_ADC_DIS_SHIFT         (1)
#define NU2115_VBUS_ADC_DIS_MASK          (BIT(0))
#define NU2115_VBUS_ADC_DIS_SHIFT         (0)

/* ADC_FN_DISABLE reg=0x16 */
#define NU2115_ADC_FN_DIS_REG             0x16
#define NU2115_ADC_FN_DIS_REG_INIT_MASTER (0x0E)
#define NU2115_ADC_FN_DIS_REG_INIT_SLAVE  (0x0E)

#define NU2115_VAC1_ADC_DIS_MASK           (BIT(7))
#define NU2115_VAC1_ADC_DIS_SHIFT          (7)
#define NU2115_VAC2_ADC_DIS_MASK           (BIT(6))
#define NU2115_VAC2_ADC_DIS_SHIFT          (6)
#define NU2115_VOUT_ADC_DIS_MASK          (BIT(5))
#define NU2115_VOUT_ADC_DIS_SHIFT         (5)
#define NU2115_VBAT_ADC_DIS_MASK          (BIT(4))
#define NU2115_VBAT_ADC_DIS_SHIFT         (4)
#define NU2115_IBAT_ADC_DIS_MASK          (BIT(3))
#define NU2115_IBAT_ADC_DIS_SHIFT         (3)
#define NU2115_TSBUS_ADC_DIS_MASK         (BIT(2))
#define NU2115_TSBUS_ADC_DIS_SHIFT        (2)
#define NU2115_TSBAT_ADC_DIS_MASK         (BIT(1))
#define NU2115_TSBAT_ADC_DIS_SHIFT        (1)
#define NU2115_TDIE_ADC_DIS_MASK          (BIT(0))
#define NU2115_TDIE_ADC_DIS_SHIFT         (0)

/* IBUS_ADC1 reg=0x17 IBUS=ibus_adc[8:0]*1ma */
/* IBUS_ADC0 reg=0x18 */
#define NU2115_IBUS_ADC1_REG              0x17
#define NU2115_IBUS_ADC0_REG              0x18

#define NU2115_IBUS_ADC_STEP              (1)     /* 1ma */

/* VBUS_ADC1 reg=0x19 VBUS=vbus_adc[8:0]*1mv */
/* VBUS_ADC0 reg=0x1A */
#define NU2115_VBUS_ADC1_REG              0x19
#define NU2115_VBUS_ADC0_REG              0x1A

#define NU2115_VBUS_ADC_STEP              (1)     /* 1mv */

/* VAC1_ADC1 reg=0x1B VAC=vac_adc[8:0]*1mv */
/* VAC1_ADC0 reg=0x1C */
#define NU2115_VAC1_ADC1_REG               0x1B
#define NU2115_VAC1_ADC0_REG               0x1C

#define NU2115_VAC1_ADC_STEP               (1)     /* 1mv */

/* VAC2_ADC1 reg=0x1D VAC=vac_adc[8:0]*1mv */
/* VAC2_ADC0 reg=0x1E */
#define NU2115_VAC2_ADC1_REG               0x1D
#define NU2115_VAC2_ADC0_REG               0x1E

#define NU2115_VAC2_ADC_STEP               (1)     /* 1mv */


/* VOUT_ADC1 reg=0x1C VOUT=vout_adc[8:0]*1mv */
/* VOUT_ADC0 reg=0x1D */
#define NU2115_VOUT_ADC1_REG              0x1F
#define NU2115_VOUT_ADC0_REG              0x20

#define NU2115_VOUT_ADC_STEP              (1)     /* 1mv */

/* VBAT_ADC1 reg=0x21 VBAT=vbat_adc[8:0]*1mv */
/* VBAT_ADC0 reg=0x22 */
#define NU2115_VBAT_ADC1_REG              0x21
#define NU2115_VBAT_ADC0_REG              0x22

#define NU2115_VBAT_ADC_STEP              (1)     /* 1mv */

/* IBAT_ADC1 reg=0x23 IBAT=ibat_adc[8:0]*1ma */
/* IBAT_ADC0 reg=0x24 */
#define NU2115_IBAT_ADC1_REG              0x23
#define NU2115_IBAT_ADC0_REG              0x24

#define NU2115_IBAT_ADC_STEP              (1)     /* 1ma */

/* TSBUS_ADC1 reg=0x25 TSBUS=tsbus_adc[8:0]*0.09766% */
/* TSBUS_ADC0 reg=0x26 */
#define NU2115_TSBUS_ADC1_REG             0x25
#define NU2115_TSBUS_ADC1_MASK            (BIT(0) | BIT(1) | BIT(7))

#define NU2115_TSBUS_ADC0_REG             0x26

#define NU2115_TSBUS_ADC_STEP             (9766)  /* 0.09766% */
#define NU2115_TSBUS_PER_MAX              (10000000)

/* TSBAT_ADC1 reg=0x27 TSBAT=tsbat_adc[8:0]*0.09766% */
/* TSBAT_ADC0 reg=0x28 */
#define NU2115_TSBAT_ADC1_REG             0x27
#define NU2115_TSBAT_ADC1_MASK            (BIT(0) | BIT(1) | BIT(7))

#define NU2115_TSBAT_ADC0_REG             0x28

#define NU2115_TSBAT_ADC_STEP             (9766) /* 0.09766% */


/* TDIE_ADC1 reg=0x29 TDIE=tdie_adc[8:0]*0.5c */
/* TDIE_ADC0 reg=0x2A */
#define NU2115_TDIE_ADC1_REG              0x29
#define NU2115_TDIE_ADC1_MASK             (BIT(0) | BIT(7))

#define NU2115_TDIE_ADC0_REG              0x2A

#define NU2115_TDIE_ADC_STEP              (5)        /* 0.5c */


/* TSBUS_FLT1 reg=0x2B
 * default=4.1% [b0001 0101], TSBUS_FLT=tsbus_flt[7:0] * 0.19531%
 */
#define NU2115_TSBUS_FLT_REG              0x2B

#define NU2115_TSBUS_FLT_MAX              (4980405)  /* 49.80405% */
#define NU2115_TSBUS_FLT_BASE             (0)
#define NU2115_TSBUS_FLT_DEFAULT          (410151)   /* 4.10151% */
#define NU2115_TSBUS_FLT_STEP             (19531)    /* 0.19531% */

/* TSBAT_FLT0 reg=0x2C
 * default=4.1% [b0001 0101],  TSBUS_FLT=tsbus_flt[7:0] * 0.19531%
 */
#define NU2115_TSBAT_FLT_REG              0x2C

#define NU2115_TSBAT_FLT_MAX              (4980405)  /* 49.80405% */
#define NU2115_TSBAT_FLT_BASE             (0)
#define NU2115_TSBAT_FLT_DEFAULT          (410151)   /* 4.10151% */
#define NU2115_TSBAT_FLT_STEP             (19531)    /* 0.19531% */


/* TDIE_ALM reg=0x2d
 * default=125c [b1100 1000], TDIE_ALM=25+tdie_alm[7:0]*0.5c
 */
#define NU2115_TDIE_ALM_REG               0x2D

#define NU2115_TDIE_ALM_MAX               (1500)     /* 150c */
#define NU2115_TDIE_ALM_BASE              (225)      /* 22.5c */
#define NU2115_TDIE_ALM_DEFAULT           (1200)     /* 12c */
#define NU2115_TDIE_ALM_STEP              (5)        /* 0.5c */

/* DEGLITCH reg=0x2e */
#define NU2115_DEGLITCH_REG               0x2E
#define NU2115_MS_MASK                    (BIT(3) | BIT(4))
#define NU2115_MS_SHIFT                   (3)
#define NU2115_IBAT_SNS_RES_MASK           (BIT(1))
#define NU2115_IBAT_SNS_RES_SHIFT          (1)
#define NU2115_IBAT_SNS_RESISTOR_5MOHM          1
#define NU2115_IBAT_SNS_RESISTOR_2MOHM          0

#define NU2115_PRESENT_DET_REG               0x2F
#define NU2115_EN_OTG_MASK           (BIT(1))
#define NU2115_EN_OTG_SHIFT          (1)

#define NU2115_ACDRV_CTRL_REG               0x30
#define NU2115_EN_ACDRV1_MASK           (BIT(7))
#define NU2115_EN_ACDRV1_SHIFT           (7)
#define NU2115_EN_ACDRV2_MASK           (BIT(6))
#define NU2115_EN_ACDRV2_SHIFT           (6)

/* PART_INFO reg=0x31 */
#define NU2115_PART_INFO_REG              0x31

#define NU2115_DEVICE_REV_MASK            (BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define NU2115_DEVICE_REV_SHIFT           (4)
#define NU2115_DEVICE_ID_MASK             (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define NU2115_DEVICE_ID_SHIFT            (0)

#define NU2115_DEVICE_ID_NU2115          (0x00)
#define NU2115_DEVICE_ID_SY6537C          (0x0E)


#endif /* end of _NU2115_H_ */
