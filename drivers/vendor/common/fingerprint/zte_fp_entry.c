/****************************

       zte_fp_entry.c

****************************/

#include "zte_fp_entry.h"

#define ZTE_FP_DEV_NAME     "zte,fingerprint"

struct zte_fp_data {
	struct platform_device *fp_dev;
	struct pinctrl *fp_pinctrl;
	struct pinctrl_state *mode_spi;
	struct pinctrl_state *mode_gpio;
};

struct zte_fp_data fp_data;

static const struct of_device_id zte_fp_match_table[] = {
	{ .compatible = ZTE_FP_DEV_NAME },
	{},
};

int zte_fp_pinctrl_select_spi(bool is_spi_mode)
{
	struct pinctrl_state *mode_tmp = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(fp_data.fp_pinctrl)
		|| IS_ERR_OR_NULL(fp_data.mode_spi)
		|| IS_ERR_OR_NULL(fp_data.mode_gpio)) {
		zte_fp_log(INFO_LOG, "%s:pinctrl not config\n", __func__);
		return 0;
	}

	mode_tmp = (is_spi_mode) ? fp_data.mode_spi : fp_data.mode_gpio;

	ret = pinctrl_select_state(fp_data.fp_pinctrl, mode_tmp);
	if (ret < 0) {
		zte_fp_log(INFO_LOG, "%s:failed to select pin to %s state\n", __func__, (is_spi_mode) ? "spi" : "gpio");
	} else {
		zte_fp_log(INFO_LOG, "%s:success to select pin to %s state\n", __func__, (is_spi_mode) ? "spi" : "gpio");
	}

	return 0;
}

static int zte_fp_probe(struct platform_device *fp_dev)
{
	fp_data.fp_pinctrl = devm_pinctrl_get(&(fp_dev->dev));
	if (IS_ERR_OR_NULL(fp_data.fp_pinctrl)) {
		zte_fp_log(INFO_LOG, "%s:devm_pinctrl_get failed\n", __func__);
		goto Failed_loop;
	}

	zte_fp_log(INFO_LOG, "%s:devm_pinctrl_get success\n", __func__);

	fp_data.mode_spi = pinctrl_lookup_state(fp_data.fp_pinctrl, "zte_fp_spi");
	if (IS_ERR_OR_NULL(fp_data.mode_spi)) {
		zte_fp_log(INFO_LOG, "%s:pinctrl_lookup_state spi failed\n", __func__);
		goto Failed_loop;
	}

	zte_fp_log(INFO_LOG, "%s:pinctrl_lookup_state spi success\n", __func__);

	fp_data.mode_gpio = pinctrl_lookup_state(fp_data.fp_pinctrl, "zte_fp_gpio");
	if (IS_ERR_OR_NULL(fp_data.mode_gpio)) {
		zte_fp_log(INFO_LOG, "%s:pinctrl_lookup_state gpio failed\n", __func__);
	} else {
		zte_fp_log(INFO_LOG, "%s:pinctrl_lookup_state gpio success\n", __func__);
	}

	zte_fp_log(INFO_LOG, "%s:success to select pin to spi state  wwww\n", __func__);

	return 0;

Failed_loop:
	zte_fp_log(INFO_LOG, "%s:failed to probe\n", __func__);

	return 0;
}

static int zte_fp_remove(struct platform_device *fp_dev)
{
	devm_pinctrl_put(fp_data.fp_pinctrl);

	return 0;
}

static struct platform_driver zte_fp_driver = {
	.driver = {
		.name = ZTE_FP_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = zte_fp_match_table,
	},
	.probe = zte_fp_probe,
	.remove = zte_fp_remove,
};

static int __init zte_fp_init(void)
{
	int ret = 0;
	zte_fp_log(INFO_LOG, "[%s]enter! driver_time:2023-09-25\n", __func__);

	platform_driver_register(&zte_fp_driver);

#ifdef CONFIG_PLATFORM_FINGERPRINT_CDFINGER
	zte_fp_log(INFO_LOG, "cdfinger_fp_init enter!\n");
	cdfinger_fp_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_CHIPONE
	zte_fp_log(INFO_LOG, "fpsensor_init enter!\n");
	fpsensor_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SUNWAVE
	zte_fp_log(INFO_LOG, "sf_ctl_driver_init enter!\n");
	sf_ctl_driver_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_GOODIX
	zte_fp_log(INFO_LOG, "goodix_driver_init enter!\n");
	gf_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC
	zte_fp_log(INFO_LOG, "fpc_sensor_init enter!\n");
	fpc_sensor_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC1020
	zte_fp_log(INFO_LOG, "fpc_init enter!\n");
	fpc_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SILEAD
	zte_fp_log(INFO_LOG, "silfp_dev_init enter!\n");
	silfp_dev_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH
	zte_fp_log(INFO_LOG, "focaltech_fp_driver_init enter!\n");
	focaltech_fp_driver_init();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2
	zte_fp_log(INFO_LOG, "focaltech_fp_driver_v2_init enter!\n");
	focaltech_fp_driver_v2_init();
#endif

	ret = zte_fp_debug_proc_init();
	if (ret) {
		zte_fp_log(ERR_LOG, "zte_fp_debug_proc_init failed\n");
		//return ret;
	} else {
		zte_fp_log(INFO_LOG, "zte_fp_debug_proc_init success\n");
	}

	ret = zte_fp_debug_gpio_init();
	if (ret) {
		zte_fp_log(ERR_LOG, "zte_fp_gpio_init failed\n");
		//return ret;
	} else {
		zte_fp_log(INFO_LOG, "zte_fp_gpio_init success\n");
	}

	zte_fp_log(INFO_LOG, "[%s]exit!\n", __func__);
	return 0;
}


static void __exit zte_fp_exit(void)
{
	int ret = 0;
	zte_fp_log(INFO_LOG, "[%s]enter!\n", __func__);

#ifdef CONFIG_PLATFORM_FINGERPRINT_CDFINGER
	zte_fp_log(INFO_LOG, "cdfinger_fp_exit enter!\n");
	cdfinger_fp_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_CHIPONE
	zte_fp_log(INFO_LOG, "fpsensor_exit enter!\n");
	fpsensor_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SUNWAVE
	zte_fp_log(INFO_LOG, "sf_ctl_driver_exit enter!\n");
	sf_ctl_driver_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_GOODIX
	zte_fp_log(INFO_LOG, "goodix_driver_exit enter!\n");
	gf_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC
	zte_fp_log(INFO_LOG, "fpc_sensor_exit enter!\n");
	fpc_sensor_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FPC1020
	zte_fp_log(INFO_LOG, "fpc_exit enter!\n");
	fpc_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_SILEAD
	zte_fp_log(INFO_LOG, "silfp_dev_exit enter!\n");
	silfp_dev_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH
	zte_fp_log(INFO_LOG, "focaltech_fp_driver_exit enter!\n");
	focaltech_fp_driver_exit();
#endif

#ifdef CONFIG_PLATFORM_FINGERPRINT_FOCALTECH_V2
	zte_fp_log(INFO_LOG, "focaltech_fp_driver_v2_exit enter!\n");
	focaltech_fp_driver_v2_exit();
#endif

	ret = zte_fp_debug_proc_deinit();
	if (ret) {
		zte_fp_log(ERR_LOG, "zte_fp_debug_proc_deinit failed\n");
	} else {
		zte_fp_log(INFO_LOG, "zte_fp_debug_proc_deinit success\n");
	}

	zte_fp_debug_gpio_deinit();

	platform_driver_unregister(&zte_fp_driver);

	zte_fp_log(INFO_LOG, "[%s]exit!\n", __func__);
}

module_init(zte_fp_init);
module_exit(zte_fp_exit);

MODULE_DESCRIPTION("ZTE Fp Driver Entry");
MODULE_AUTHOR("***@zte.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ZTE");


