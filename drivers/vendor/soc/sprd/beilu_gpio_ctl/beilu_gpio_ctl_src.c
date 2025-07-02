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


#define ZTE_BEILU_PROC_DIR "bl_gpios"
#define ZTE_BEILU_MCU_RESET "mcu_reset"
#define ZTE_BEILU_MCU_IRQ "mcu_irq"
struct proc_dir_entry *beilu_proc_dir = NULL;
static int  mcu_reset_gpio, mcu_en_gpio, mcu_irq_gpio, mcu_ready_gpio, mcu_ready_int;
struct mutex mcu_reset_lock;
static struct wakeup_source *bl_mcu_ready_wakelock;

static ssize_t mcu_reset_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char reset_flag[4];
	int reset_gpio_value = 0;

	if( *offset > 0)
		return 0;
	mutex_lock(&mcu_reset_lock);
	reset_gpio_value = gpio_get_value(mcu_reset_gpio);
	snprintf(reset_flag, sizeof(reset_flag), "%d", reset_gpio_value);
	ret = copy_to_user(buffer, &reset_flag, sizeof(reset_flag));
	if (ret < 0) {
		log_err("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(reset_flag);
	*offset += ret;
	mutex_unlock(&mcu_reset_lock);

	log_debug("count:%d ret:%d, reset_flag:%s,offset:%lld\n", count, ret, reset_flag, *offset);
	return ret;
}

static ssize_t mcu_reset_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char reset_flag[8];

	mutex_lock(&mcu_reset_lock);
	ret = copy_from_user(reset_flag, buffer, sizeof(reset_flag));
	if (ret < 0) {
		log_err("failed to copy data from user space!\n");
		return ret;
	}
	log_debug("reset_flag is %s, count is %d, ret is %d.\n", reset_flag, count, ret);
	if(reset_flag[0] == '1') {
		log_err("pls reset mcu!!!\n");
		gpio_set_value(mcu_reset_gpio, 1);
		gpio_set_value(mcu_en_gpio, 1);
		msleep(20);
		gpio_set_value(mcu_reset_gpio, 0);
		gpio_set_value(mcu_en_gpio, 0);
	}
	mutex_unlock(&mcu_reset_lock);

	return count;
}

static ssize_t mcu_irq_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char irq_flag[4];
	int irq_gpio_value = 0;

	if( *offset > 0)
		return 0;
	mutex_lock(&mcu_reset_lock);
	irq_gpio_value = gpio_get_value(mcu_irq_gpio);
	snprintf(irq_flag, sizeof(irq_flag), "%d", irq_gpio_value);
	ret = copy_to_user(buffer, &irq_flag, sizeof(irq_flag));
	if (ret < 0) {
		log_err("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(irq_flag);
	*offset += ret;
	mutex_unlock(&mcu_reset_lock);

	log_debug("count:%d ret:%d, irq_flag:%s,offset:%lld\n", count, ret, irq_flag, *offset);
	return ret;
}

static ssize_t mcu_irq_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char irq_flag[8];

	mutex_lock(&mcu_reset_lock);
	ret = copy_from_user(irq_flag, buffer, sizeof(irq_flag));
	if (ret < 0) {
		log_err("failed to copy data from user space!\n");
		return ret;
	}
	log_debug("irq_flag is %s, count is %d, ret is %d.\n", irq_flag, count, ret);
	if(irq_flag[0] == '1') {
		log_err("pls sw reset mcu!!!\n");
		gpio_set_value(mcu_irq_gpio, 1);
		msleep(20);
		gpio_set_value(mcu_irq_gpio, 0);
	}
	mutex_unlock(&mcu_reset_lock);

	return count;
}

static const struct file_operations proc_ops_mcu_reset = {
	.owner = THIS_MODULE,
	.read = mcu_reset_read,
	.write = mcu_reset_write,
};
static const struct file_operations proc_ops_mcu_irq = {
	.owner = THIS_MODULE,
	.read = mcu_irq_read,
	.write = mcu_irq_write,
};

static irqreturn_t mcu_ready_handler(int irq, void *data) {
	int ready_gpio_value = 0;
	long timeout = 2 * HZ;

	ready_gpio_value = gpio_get_value(mcu_ready_gpio);
	if (ready_gpio_value) {
		log_debug("mcu_ready irq!!\n");
	}
	__pm_wakeup_event(bl_mcu_ready_wakelock, jiffies_to_msecs(timeout));
	msleep(1000);
	return IRQ_HANDLED;
}

static int beilu_gpio_init() {
	int ret = 0;
	struct device_node *node;

	log_debug("%s: enter \n", __FUNCTION__);
	node = of_find_node_with_property(NULL, "beilumcu-reset-gpio");
	if(node) {
		mcu_reset_gpio = of_get_named_gpio(node, "beilumcu-reset-gpio", 0);
	}
	ret = gpio_request(mcu_reset_gpio, "mcu_reset_gpio");
	if (ret < 0) {
		log_err("mcu reset gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(mcu_reset_gpio,0);
	if (ret < 0) {
		log_err("mcu reset gpio set dir failed!\n");
		return ret;
	}

	node = of_find_node_with_property(NULL, "beilumcu-en-gpio");
	if(node) {
		mcu_en_gpio = of_get_named_gpio(node, "beilumcu-en-gpio", 0);
	}
	ret = gpio_request(mcu_en_gpio, "mcu_en_gpio");
	if (ret < 0) {
		log_err("mcu en gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(mcu_en_gpio,0);
	if (ret < 0) {
		log_err("mcu en gpio set dir failed!\n");
		return ret;
	}

	node = of_find_node_with_property(NULL, "beilumcu-irq-gpio");
	if(node) {
		mcu_irq_gpio = of_get_named_gpio(node, "beilumcu-irq-gpio", 0);
	}
	ret = gpio_request(mcu_irq_gpio, "mcu_irq_gpio");
	if (ret < 0) {
		log_err("mcu irq gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(mcu_irq_gpio,0);
	if (ret < 0) {
		log_err("mcu irq gpio set dir failed!\n");
		return ret;
	}

	node = of_find_node_with_property(NULL, "beilumcu-ready-gpio");
	if(node) {
		mcu_ready_gpio = of_get_named_gpio(node, "beilumcu-ready-gpio", 0);
	}
	ret = gpio_request(mcu_ready_gpio, "mcu_ready_gpio");
	if (ret < 0) {
		log_err("mcu ready gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_input(mcu_ready_gpio);
	if (ret < 0) {
		log_err("mcu ready gpio set dir failed!\n");
		return ret;
	}
	mcu_ready_int = gpio_to_irq(mcu_ready_gpio);
	if(mcu_ready_int < 0) {
		log_err("mcu ready int get failed!\n");
		return -EINVAL;
	}
	ret = request_threaded_irq(mcu_ready_int, NULL, mcu_ready_handler, IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "mcu_ready_irq", NULL);
	if(ret) {
		log_err("mcu request ready irq failed!\n");
		return ret;
	}

	return 0;
}

static void create_beilu_proc_entry(void) {
	struct proc_dir_entry *mcu_reset_proc_entry = NULL;
	struct proc_dir_entry *mcu_irq_proc_entry = NULL;

	beilu_proc_dir = proc_mkdir(ZTE_BEILU_PROC_DIR, NULL);
	if(beilu_proc_dir == NULL) {
		log_err("mkdir bl_gpios failed!\n");
		return;
	}

	mcu_reset_proc_entry = proc_create(ZTE_BEILU_MCU_RESET, 0666, beilu_proc_dir, &proc_ops_mcu_reset);
	if (mcu_reset_proc_entry == NULL) {
		log_err("Failed to create mcu reset entry\n");
		return;
	}
	mcu_irq_proc_entry = proc_create(ZTE_BEILU_MCU_IRQ, 0666, beilu_proc_dir, &proc_ops_mcu_irq);
	if (mcu_irq_proc_entry == NULL) {
		log_err("Failed to create mcu irq entry\n");
		return;
	}
	return;
}

static int __init beilu_gpio_ctl_init(void) {
	log_debug("%s:enter \n", __FUNCTION__);
	beilu_gpio_init();
	create_beilu_proc_entry();
	mutex_init(&mcu_reset_lock);
	bl_mcu_ready_wakelock = wakeup_source_create("bl_mcu_ready_wakelock");
	wakeup_source_add(bl_mcu_ready_wakelock);
	log_debug("%s:done \n", __FUNCTION__);
	return 0;
}

static void __exit beilu_gpio_ctl_exit(void) {
	if(beilu_proc_dir == NULL) {
		log_err("proc/bl_gpios is null\n");
		return;
	}
	remove_proc_entry(ZTE_BEILU_MCU_RESET, beilu_proc_dir);
	remove_proc_entry(ZTE_BEILU_PROC_DIR, NULL);
	wakeup_source_remove(bl_mcu_ready_wakelock);
}

module_init(beilu_gpio_ctl_init);
module_exit(beilu_gpio_ctl_exit);

MODULE_AUTHOR("zte light");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("zte bl gpio ctl");