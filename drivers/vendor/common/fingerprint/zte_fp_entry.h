/****************************

       zte_fp_entry.h

****************************/

#ifndef __ZTE_FP_ENTRY__H__
#define __ZTE_FP_ENTRY__H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>

#define ZLOG_MOD_NAME	"[ZTE_LDD_FP]"
#define ZLOG_VENDOR_NAME	"[ENTRY]"
typedef enum {
    ERR_LOG = 0,
    WARN_LOG,
    INFO_LOG,
    DEBUG_LOG,
    ALL_LOG,
} zte_fp_log_level_t;

extern int zte_fp_log_level;

#define zte_fp_log(level, fmt, args...) do { \
			if (zte_fp_log_level >= level) {\
				pr_err("%s%s"fmt, ZLOG_MOD_NAME, ZLOG_VENDOR_NAME, ##args); \
			} \
		} while (0)

#ifdef CONFIG_PLATFORM_FINGERPRINT_CDFINGER
extern int cdfinger_fp_init(void);
extern void cdfinger_fp_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_CHIPONE
extern int fpsensor_init(void);
extern void fpsensor_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SUNWAVE
extern int sf_ctl_driver_init(void);
extern void sf_ctl_driver_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_GOODIX
extern int gf_init(void);
extern void gf_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC
extern int fpc_sensor_init(void);
extern void fpc_sensor_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC1020
extern int fpc_init(void);
extern void fpc_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SILEAD
extern int silfp_dev_init(void);
extern void silfp_dev_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH
extern int focaltech_fp_driver_init(void);
extern void focaltech_fp_driver_exit(void);
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2
extern int focaltech_fp_driver_v2_init(void);
extern void focaltech_fp_driver_v2_exit(void);
#endif

/*zte_fp_debug*/
int zte_fp_debug_proc_init(void);
int zte_fp_debug_proc_deinit(void);
int zte_fp_debug_gpio_init(void);
int zte_fp_debug_gpio_deinit(void);

#endif
