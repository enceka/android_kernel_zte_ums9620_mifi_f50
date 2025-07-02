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

//#define NTAG_GPIO_DEBUG
#ifdef NTAG_GPIO_DEBUG
#define BL_DEBUG(fmt, args...) do { \
	printk(KERN_ERR "[bl ntag] %s:"fmt, __func__, ##args);\
} while (0)
#else
#define BL_DEBUG(fmt, args...)
#endif
#define BL_ERR(fmt, args...) do { \
	printk(KERN_ERR "[bl ntag] %s:"fmt, __func__, ##args);\
} while (0)

#define ZTE_NTAG_PROC_DIR "ntag_gpios"
#define ZTE_NTAG_RESET "ntag_reset"
#define ZTE_NTAG_IRQ "ntag_irq"
struct proc_dir_entry *ntag_proc_dir = NULL;
static int  ntag_reset_gpio, ntag_irq_gpio, ntag_ready_int;
struct mutex ntag_reset_lock;
static struct wakeup_source *bl_ntag_ready_wakelock;

static ssize_t ntag_reset_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char reset_flag[4];
	int reset_gpio_value = 0;

	BL_DEBUG("enter\n");
	if( *offset > 0)
		return 0;
	mutex_lock(&ntag_reset_lock);
	reset_gpio_value = gpio_get_value(ntag_reset_gpio);
	if (reset_gpio_value == 0) {
	    snprintf(reset_flag, sizeof(reset_flag), "%d", 1);
	} else {
	    snprintf(reset_flag, sizeof(reset_flag), "%d", 0);
	}
	ret = copy_to_user(buffer, &reset_flag, sizeof(reset_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(reset_flag);
	*offset += ret;
	mutex_unlock(&ntag_reset_lock);

	BL_DEBUG("count:%d ret:%d, reset_flag:%s,offset:%lld\n", count, ret, reset_flag, *offset);
	BL_DEBUG("finished\n");
	return ret;
}

static ssize_t ntag_reset_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char reset_flag[8];

	BL_DEBUG("enter\n");
	mutex_lock(&ntag_reset_lock);
	ret = copy_from_user(reset_flag, buffer, sizeof(reset_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data from user space!\n");
		return ret;
	}
	BL_DEBUG("reset_flag is %s, count is %d, ret is %d.\n", reset_flag, count, ret);
	if(reset_flag[0] == '1') {
		BL_ERR("set reset ntag!!!\n");
		gpio_set_value(ntag_reset_gpio, 0);
	}
	if(reset_flag[0] == '0') {
		BL_ERR("disable reset ntag!!!\n");
		gpio_set_value(ntag_reset_gpio, 1);
	}
	mutex_unlock(&ntag_reset_lock);
	BL_DEBUG("finished!\n");

	return count;
}

static ssize_t ntag_irq_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char irq_flag[4];
	int irq_gpio_value = 0;

	BL_DEBUG("enter\n");
	if( *offset > 0)
		return 0;
	mutex_lock(&ntag_reset_lock);
	irq_gpio_value = gpio_get_value(ntag_irq_gpio);
	if (irq_gpio_value == 0) {
	    snprintf(irq_flag, sizeof(irq_flag), "%d", 1);
	} else {
	    snprintf(irq_flag, sizeof(irq_flag), "%d", 0);
	}
	ret = copy_to_user(buffer, &irq_flag, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(irq_flag);
	*offset += ret;
	mutex_unlock(&ntag_reset_lock);

	BL_DEBUG("count:%d ret:%d, irq_flag:%s,irq_gpio_value:%d,offset:%lld\n", count, ret, irq_flag, irq_gpio_value, *offset);
	BL_DEBUG("finished\n");
	return ret;
}

static ssize_t ntag_irq_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char irq_flag[8];

	BL_DEBUG("enter\n");
	mutex_lock(&ntag_reset_lock);
	ret = copy_from_user(irq_flag, buffer, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data from user space!\n");
		return ret;
	}
	BL_DEBUG("irq_flag is %s, count is %d, ret is %d.\n", irq_flag, count, ret);
	if(irq_flag[0] == '1') {
		BL_ERR("set irq ntag!!!\n");
		gpio_set_value(ntag_irq_gpio, 0);
	}
	if(irq_flag[0] == '0') {
		BL_ERR(" clr irq ntag!!!\n");
		gpio_set_value(ntag_irq_gpio, 1);
	}
	mutex_unlock(&ntag_reset_lock);
	BL_DEBUG("finished!\n");

	return count;
}

static const struct file_operations proc_ops_ntag_reset = {
	.owner = THIS_MODULE,
	.read = ntag_reset_read,
	.write = ntag_reset_write,
};
static const struct file_operations proc_ops_ntag_irq = {
	.owner = THIS_MODULE,
	.read = ntag_irq_read,
	.write = ntag_irq_write,
};

static irqreturn_t ntag_ready_handler(int irq, void *data) {
	int ready_gpio_value = 0;
	long timeout = 2 * HZ;

	ready_gpio_value = gpio_get_value(ntag_irq_gpio);
	if (!ready_gpio_value) {
		BL_DEBUG("ntag_ready irq!!\n");
	}
	__pm_wakeup_event(bl_ntag_ready_wakelock, jiffies_to_msecs(timeout));
	msleep(1000);
	return IRQ_HANDLED;
}

static int ntag_gpio_init() {
	int ret = 0;
	struct device_node *node;

	BL_DEBUG("enter \n");
	node = of_find_node_with_property(NULL, "ntag-reset-gpio");
	if (node < 0) {
		BL_ERR("ntag reset gpio get failed!\n");
	}
	if(node) {
		ntag_reset_gpio = of_get_named_gpio(node, "ntag-reset-gpio", 0);
	}
	if (ntag_reset_gpio < 0) {
		BL_ERR("ntag reset gpio get from dts failed!\n");
	}
	ret = gpio_request(ntag_reset_gpio, "ntag_reset_gpio");
	if (ret < 0) {
		BL_ERR("ntag reset gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(ntag_reset_gpio,0);
	if (ret < 0) {
		BL_ERR("ntag reset gpio set dir failed!\n");
		return ret;
	}

	node = of_find_node_with_property(NULL, "ntag-irq-gpio");
	if(node) {
		ntag_irq_gpio = of_get_named_gpio(node, "ntag-irq-gpio", 0);
	}
	ret = gpio_request(ntag_irq_gpio, "ntag_irq_gpio");
	if (ret < 0) {
		BL_ERR("ntag irq gpio request failed!\n");
		return ret;
	}

	ret = gpio_direction_input(ntag_irq_gpio);
	if (ret < 0) {
		BL_ERR("ntag irq gpio set dir failed!\n");
		return ret;
	}
	ntag_ready_int = gpio_to_irq(ntag_irq_gpio);
	if(ntag_ready_int < 0) {
		BL_ERR("ntag ready int get failed!\n");
		return -EINVAL;
	}
	ret = request_threaded_irq(ntag_ready_int, NULL, ntag_ready_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT, "ntag_ready_irq", NULL);
	if(ret) {
		BL_ERR("ntag request ready irq failed!\n");
		return ret;
	}

	return 0;
}

static void create_ntag_proc_entry(void) {
	struct proc_dir_entry *ntag_reset_proc_entry = NULL;
	struct proc_dir_entry *ntag_irq_proc_entry = NULL;

	BL_DEBUG("enter \n");
	ntag_proc_dir = proc_mkdir(ZTE_NTAG_PROC_DIR, NULL);
	if(ntag_proc_dir == NULL) {
		BL_ERR("mkdir bl_gpios failed!\n");
		return;
	}

	ntag_reset_proc_entry = proc_create(ZTE_NTAG_RESET, 0666, ntag_proc_dir, &proc_ops_ntag_reset);
	if (ntag_reset_proc_entry == NULL) {
		BL_ERR("Failed to create ntag reset entry\n");
		return;
	}
	ntag_irq_proc_entry = proc_create(ZTE_NTAG_IRQ, 0666, ntag_proc_dir, &proc_ops_ntag_irq);
	if (ntag_irq_proc_entry == NULL) {
		BL_ERR("Failed to create ntag irq entry\n");
		return;
	}
	return;
}

static int __init ntag_gpio_ctl_init(void) {
	BL_DEBUG("enter \n");
	ntag_gpio_init();
	create_ntag_proc_entry();
	mutex_init(&ntag_reset_lock);
	bl_ntag_ready_wakelock = wakeup_source_create("bl_ntag_ready_wakelock");
	wakeup_source_add(bl_ntag_ready_wakelock);
	BL_DEBUG("done \n");
	return 0;
}

static void __exit ntag_gpio_ctl_exit(void) {
	if(ntag_proc_dir == NULL) {
		BL_ERR("proc/bl_gpios is null\n");
		return;
	}
	remove_proc_entry(ZTE_NTAG_IRQ, ntag_proc_dir);
	remove_proc_entry(ZTE_NTAG_RESET, ntag_proc_dir);
	remove_proc_entry(ZTE_NTAG_PROC_DIR, NULL);
	wakeup_source_remove(bl_ntag_ready_wakelock);
}

module_init(ntag_gpio_ctl_init);
module_exit(ntag_gpio_ctl_exit);

MODULE_AUTHOR("zte light");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("zte ntag gpio ctl");
