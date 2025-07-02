#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <asm-generic/errno.h>
#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm_wakeup.h>

#define ZTE_BSP_LOG_PREFIX "[ZTE_LDD_BOOT][GPIO]"
#define log_err(fmt, ...) pr_err(ZTE_BSP_LOG_PREFIX "[E]" fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) pr_warn(ZTE_BSP_LOG_PREFIX "[W]" fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) pr_info(ZTE_BSP_LOG_PREFIX "[I]" fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug(ZTE_BSP_LOG_PREFIX "[D]" fmt, ##__VA_ARGS__)

static int  mcu_en_gpio;

static int gpio_init() {
	int ret = 0;
	struct device_node *node;

	log_debug("%s: enter \n", __FUNCTION__);
	node = of_find_node_with_property(NULL, "beilumcu-en-gpio");
	if(node) {
		mcu_en_gpio = of_get_named_gpio(node, "beilumcu-en-gpio", 0);
	}
	ret = gpio_request(mcu_en_gpio, "mcu_en_gpio");
	if (ret < 0) {
		log_err("mcu en gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(mcu_en_gpio,1);
	if (ret < 0) {
		log_err("mcu en gpio set dir failed!\n");
		return ret;
	}

	return 0;
}

static int __init gpio_ctl_init(void) {
	log_debug("%s: enter \n", __FUNCTION__);
	gpio_init();
	log_debug("%s:done \n", __FUNCTION__);
	return 0;
}

static void __exit gpio_ctl_exit(void) {
}

module_init(gpio_ctl_init);
module_exit(gpio_ctl_exit);

MODULE_AUTHOR("zte light");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("zte gpio ctl");