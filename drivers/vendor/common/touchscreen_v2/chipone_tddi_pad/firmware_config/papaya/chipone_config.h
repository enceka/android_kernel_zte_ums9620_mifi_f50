#ifndef _CHIPONE_CONFIG_H_
#define _CHIPONE_CONFIG_H_

#define CTS_MODULE1_ID                         0x0001
#define CTS_MODULE2_ID                         0x0002
#define CTS_MODULE3_ID                         0x0003

#define CTS_MODULE1_LCD_NAME                   "lead"
#define CTS_MODULE2_LCD_NAME                   "Unknown"
#define CTS_MODULE3_LCD_NAME                   "Unknown"

/*default i2c*/
#define USE_SPI_BUS
#ifdef USE_SPI_BUS
/*define use spi num*/
#define SPI_NUM                                    1
#endif

#define CTS_ROW_NUM   48
#define CTS_COL_NUM   32

#define CTS_DEFAULT_FIRMWARE        "cts_10_95_default_firmware"
#define CTS_REPORT_BY_ZTE_ALGO

#define CONFIG_CTS_CHARGER_DETECT

#define CFG_CTS_HEADSET_DETECT

#define CFG_CTS_ROTATION

#define CFG_CTS_GESTURE

#define CFG_CTS_FOR_GKI

#define CONFIG_CTS_PM_GENERIC


#endif /* _CHIPONE_CONFIG_H_ */