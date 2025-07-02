#ifndef _CHIPONE_CONFIG_H_
#define _CHIPONE_CONFIG_H_

#define CTS_MODULE1_ID                         0x0001
#define CTS_MODULE2_ID                         0x0002
#define CTS_MODULE3_ID                         0x0003

#define CTS_MODULE1_LCD_NAME                   "easyquick"
#define CTS_MODULE2_LCD_NAME                   "Unknown"
#define CTS_MODULE3_LCD_NAME                   "Unknown"

/*default i2c*/
#define USE_SPI_BUS
#ifdef USE_SPI_BUS
/*define use spi num*/
#define SPI_NUM                                    3
#endif

#define CTS_DEFAULT_FIRMWARE        "cts_6_58_default_firmware"
#define CTS_REPORT_BY_ZTE_ALGO
/* #define CTS_LCD_OPERATE_TP_RESET */

#define CONFIG_CTS_CHARGER_DETECT

#define CFG_CTS_HEADSET_DETECT

#define CFG_CTS_ROTATION

#define CFG_CTS_GESTURE

#define CFG_CTS_FOR_GKI

/** 1: hwid register addr 0x70000
 *  2: Use spi drw protocol */
#define CONFIG_CTS_ICTYPE_ICNL9922C

#endif /* _CHIPONE_CONFIG_H_ */