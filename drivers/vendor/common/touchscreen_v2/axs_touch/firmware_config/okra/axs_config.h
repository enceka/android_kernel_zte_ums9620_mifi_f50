/*
 * AXS touchscreen driver.
 *
 * Copyright (c) 2020-2023 AiXieSheng Technology. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_AXS_CONFIG_H_
#define _LINUX_AXS_CONFIG_H_


#define AXS_I2C_SLAVE_ADDR          0x3B

#define AXS_MT_PROTOCOL_B_EN        1

#define AXS_REPORT_PRESSURE_EN      0

#define AXS_GESTURE_EN              0

#define AXS_PROXIMITY_SENSOR_EN     1
#define HUB_TP_PS_ENABLE    1

#define AXS_HEADSET_EN              1

#define AXS_CHARGE_EN               1

#define AXS_ROTATION_EN             1

#define AXS_SELF_TEST_EN             1
//#define HAVE_TOUCH_KEY

#define AXS_DEBUG_LOG_EN               1
#define AXS_POWER_SOURCE_CUST_EN        0

#define AXS_DOWNLOAD_APP_EN             1
#define AXS_AUTO_DOWNLOAD_FILE_TYPE		1/*0:.i 1:.bin*/

#define AXS_AUTO_UPGRADE_EN             0
#define AXS_UPGRADE_CHECK_VERSION       0

#define AXS_BUS_IIC                     0
#define AXS_BUS_SPI                     1

#define SCREEN_MAX_X    480
#define SCREEN_MAX_Y    854

#define  RX_NUM 30
#define  TX_NUM 16
/*开启手势时，(AXS_GESTURE_EN宏开关时),AXS_DEBUG_SYSFS_EN必须要打开*/
#define AXS_DEBUG_SYSFS_EN              1
#if AXS_DEBUG_SYSFS_EN
#define AXS_FW_MAX_FILE_NAME_LENGTH     128
#endif
#define AXS_DEBUG_PROCFS_EN             1

#define AXS_CMD_MAX_WRITE_BUF_LEN       1024
#define AXS_CMD_MAX_READ_BUF_LEN        1024

#define AXS_ESD_CHECK_EN                1
#define AXS_PEN_EVENT_CHECK_EN          0
#define AXS_POWERUP_CHECK_EN            0
#define AXS_INTERRUPT_THREAD            1
#define AXS_FIRMWARE_LOG_EN            	0
#define AXS_FIRMWARE_LOG_LEN            2*1024

#define AXS_REPORT_BY_ZTE_ALGO
#define AXS_VENDOR_ID_0 0
#define AXS_VENDOR_ID_1 1
#define AXS_VENDOR_ID_2 2
#define AXS_VENDOR_ID_3 3

#define AXS_VENDOR_0_NAME                         "easyquick"
#define AXS_VENDOR_1_NAME                         "lectron"
#define AXS_VENDOR_2_NAME                         "ykl_new"
#define AXS_VENDOR_3_NAME                         "lianchuang_new"
#define AXS_DEFAULT_FIRMWARE        "axs_4_96_default_firmware"
#endif /* _LINUX_AXS_CONFIG_H_ */

