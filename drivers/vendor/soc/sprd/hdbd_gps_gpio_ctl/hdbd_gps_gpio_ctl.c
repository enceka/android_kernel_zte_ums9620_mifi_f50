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

#define GPS_GPIO_DEBUG 
//#define GPS_GPIO_DEBUG
#ifdef GPS_GPIO_DEBUG
#define GPS_DEBUG(fmt, args...) do { \
	printk(KERN_ERR "[GPS mcu] %s:"fmt, __func__, ##args);\
} while (0)
#else
#define GPS_DEBUG(fmt, args...)
#endif
#define BL_ERR(fmt, args...) do { \
	printk(KERN_ERR "[GPS mcu] %s:"fmt, __func__, ##args);\
} while (0)

#define ZTE_GPS_PROC_DIR "gps_gpios"
#define ZTE_GPS_MCU_RESET "gps_reset"
#define ZTE_GPS_MCU_AVDD "gps_avdd"
#define ZTE_GPS_MCU_PRTRG "gps_prtrg"
struct proc_dir_entry *gps_proc_dir = NULL;
static int  gps_reset_gpio, gps_avdd_gpio, gps_prtrg_gpio;
struct mutex mcu_reset_lock;
//static struct wakeup_source *bl_mcu_ready_wakelock;

static ssize_t gps_reset_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char reset_flag[4];
	int reset_gpio_value = 0;

	GPS_DEBUG("enter\n");
	if( *offset > 0)
		return 0;
	mutex_lock(&mcu_reset_lock);
	reset_gpio_value = gpio_get_value(gps_reset_gpio);
	snprintf(reset_flag, sizeof(reset_flag), "%d", reset_gpio_value);
	ret = copy_to_user(buffer, &reset_flag, sizeof(reset_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(reset_flag);
	*offset += ret;
	mutex_unlock(&mcu_reset_lock);

	GPS_DEBUG("count:%d ret:%d, reset_flag:%s,offset:%lld\n", count, ret, reset_flag, *offset);
	GPS_DEBUG("finished\n");
	return ret;
}

static ssize_t gps_reset_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char reset_flag[8];

	GPS_DEBUG("enter\n");
	mutex_lock(&mcu_reset_lock);
	ret = copy_from_user(reset_flag, buffer, sizeof(reset_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data from user space!\n");
		return ret;
	}
	GPS_DEBUG("reset_flag is %s, count is %d, ret is %d.\n", reset_flag, count, ret);
	if(reset_flag[0] == '1') {
		BL_ERR("pls reset mcu  111 !!!\n");
		gpio_set_value(gps_reset_gpio, 1);
		msleep(20);
	}else if(reset_flag[0] == '0') {
		BL_ERR("pls reset mcu  000 !!!\n");
		gpio_set_value(gps_reset_gpio, 0);
		msleep(20);
	}
	mutex_unlock(&mcu_reset_lock);
	GPS_DEBUG("finished!\n");

	return count;
}

static ssize_t gps_avdd_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char irq_flag[4];
	int irq_gpio_value = 0;

	GPS_DEBUG("enter\n");
	if( *offset > 0)
		return 0;
	mutex_lock(&mcu_reset_lock);
	irq_gpio_value = gpio_get_value(gps_avdd_gpio);
	snprintf(irq_flag, sizeof(irq_flag), "%d", irq_gpio_value);
	ret = copy_to_user(buffer, &irq_flag, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(irq_flag);
	*offset += ret;
	mutex_unlock(&mcu_reset_lock);

	GPS_DEBUG("count:%d ret:%d, irq_flag:%s,offset:%lld\n", count, ret, irq_flag, *offset);
	GPS_DEBUG("finished\n");
	return ret;
}

static ssize_t gps_avdd_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char irq_flag[8];

	GPS_DEBUG("enter\n");
	mutex_lock(&mcu_reset_lock);
	ret = copy_from_user(irq_flag, buffer, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data from user space!\n");
		return ret;
	}
	GPS_DEBUG("irq_flag is %s, count is %d, ret is %d.\n", irq_flag, count, ret);
	if(irq_flag[0] == '1') {
		BL_ERR("pls sw avdd mcu 111 !!!\n");
		gpio_set_value(gps_avdd_gpio, 1);
	}else 	if(irq_flag[0] == '0') {
		BL_ERR("pls sw avdd mcu 000 !!!\n");
		gpio_set_value(gps_avdd_gpio, 0);
	}
	mutex_unlock(&mcu_reset_lock);
	GPS_DEBUG("finished!\n");

	return count;
}

static ssize_t gps_prtrg_read(struct file *file,
								char __user *buffer, size_t count, loff_t * offset) {
	ssize_t ret = 0;
	unsigned char irq_flag[4];
	int irq_gpio_value = 0;

	GPS_DEBUG("enter\n");
	if( *offset > 0)
		return 0;
	mutex_lock(&mcu_reset_lock);
	irq_gpio_value = gpio_get_value(gps_prtrg_gpio);
	snprintf(irq_flag, sizeof(irq_flag), "%d", irq_gpio_value);
	ret = copy_to_user(buffer, &irq_flag, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data to user space!\n");
		return ret;
	}
	ret = sizeof(irq_flag);
	*offset += ret;
	mutex_unlock(&mcu_reset_lock);

	GPS_DEBUG("count:%d ret:%d, irq_flag:%s,offset:%lld\n", count, ret, irq_flag, *offset);
	GPS_DEBUG("finished\n");
	return ret;
}

static ssize_t gps_prtrg_write(struct file *file,
								const char __user *buffer, size_t count, loff_t *pos) {
	ssize_t ret = 0;
	unsigned char irq_flag[8];

	GPS_DEBUG("enter\n");
	mutex_lock(&mcu_reset_lock);
	ret = copy_from_user(irq_flag, buffer, sizeof(irq_flag));
	if (ret < 0) {
		BL_ERR("failed to copy data from user space!\n");
		return ret;
	}
	GPS_DEBUG("irq_flag is %s, count is %d, ret is %d.\n", irq_flag, count, ret);
	if(irq_flag[0] == '1') {
		BL_ERR("pls sw prtrg mcu 111 !!!\n");
		gpio_set_value(gps_prtrg_gpio, 1);
	}else 	if(irq_flag[0] == '0') {
		BL_ERR("pls sw prtrg mcu 000 !!!\n");
		gpio_set_value(gps_prtrg_gpio, 0);
	}
	mutex_unlock(&mcu_reset_lock);
	GPS_DEBUG("finished!\n");

	return count;
}

static const struct file_operations proc_ops_mcu_reset = {
	.owner = THIS_MODULE,
	.read = gps_reset_read,
	.write = gps_reset_write,
};
static const struct file_operations proc_ops_mcu_avdd = {
	.owner = THIS_MODULE,
	.read = gps_avdd_read,
	.write = gps_avdd_write,
};
static const struct file_operations proc_ops_mcu_prtrg = {
	.owner = THIS_MODULE,
	.read = gps_prtrg_read,
	.write = gps_prtrg_write,
};
/*
static irqreturn_t mcu_ready_handler(int irq, void *data) {
	int ready_gpio_value = 0;
	long timeout = 2 * HZ;

	ready_gpio_value = gpio_get_value(mcu_ready_gpio);
	if (ready_gpio_value) {
		GPS_DEBUG("mcu_ready irq!!\n");
	}
	__pm_wakeup_event(bl_mcu_ready_wakelock, jiffies_to_msecs(timeout));
	msleep(1000);
	return IRQ_HANDLED;
}
*/
static int gps_gpio_init() {
	int ret = 0;
	struct device_node *node;

	GPS_DEBUG("enter \n");
	node = of_find_node_with_property(NULL, "gpsmcu-avdd-gpio");
	if(node) {
		gps_avdd_gpio = of_get_named_gpio(node, "gpsmcu-avdd-gpio", 0);
		GPS_DEBUG("QGT gps_avdd_gpio:%d\n", gps_avdd_gpio);
	}
	ret = gpio_request(gps_avdd_gpio, "gps_avdd_gpio");
	if (ret < 0) {
		BL_ERR("mcu avdd gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(gps_avdd_gpio,0);
	if (ret < 0) {
		BL_ERR("mcu avdd gpio set dir failed!\n");
		return ret;
	}
	
	node = of_find_node_with_property(NULL, "gpsmcu-reset-gpio");
	if(node) {
		gps_reset_gpio = of_get_named_gpio(node, "gpsmcu-reset-gpio", 0);
		GPS_DEBUG("QGT gps_reset_gpio:%d\n", gps_reset_gpio);
	}
	ret = gpio_request(gps_reset_gpio, "gps_reset_gpio");
	if (ret < 0) {
		BL_ERR("mcu reset gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(gps_reset_gpio,0);
	if (ret < 0) {
		BL_ERR("mcu reset gpio set dir failed!\n");
		return ret;
	}

	node = of_find_node_with_property(NULL, "gpsmcu-prtrg-gpio");
	if(node) {
		gps_prtrg_gpio = of_get_named_gpio(node, "gpsmcu-prtrg-gpio", 0);
		GPS_DEBUG("QGT gps_prtrg_gpio:%d\n", gps_prtrg_gpio);
	}
	ret = gpio_request(gps_prtrg_gpio, "gps_prtrg_gpio");
	if (ret < 0) {
		BL_ERR("mcu prtrg gpio request failed!\n");
		return ret;
	}
	ret = gpio_direction_output(gps_prtrg_gpio,0);
	if (ret < 0) {
		BL_ERR("mcu prtrg gpio set dir failed!\n");
		return ret;
	}

	return 0;
}
/*void reset_chip(void){
	GPS_DEBUG("enter \n");
	msleep(100);
	gpio_set_value(gps_avdd_gpio, 1);
	msleep(100);
	gpio_set_value(gps_prtrg_gpio, 0);
	msleep(100);
	gpio_set_value(gps_prtrg_gpio, 1);
	GPS_DEBUG("exit \n");
}*/
static void create_gps_proc_entry(void) {
	struct proc_dir_entry *mcu_reset_proc_entry = NULL;
	struct proc_dir_entry *mcu_avdd_proc_entry = NULL;
	struct proc_dir_entry *mcu_prtrg_proc_entry = NULL;

	GPS_DEBUG("enter \n");
	gps_proc_dir = proc_mkdir(ZTE_GPS_PROC_DIR, NULL);
	if(gps_proc_dir == NULL) {
		BL_ERR("mkdir GPS_gpios failed!\n");
		return;
	}

	mcu_reset_proc_entry = proc_create(ZTE_GPS_MCU_RESET, 0666, gps_proc_dir, &proc_ops_mcu_reset);
	if (mcu_reset_proc_entry == NULL) {
		BL_ERR("Failed to create mcu reset entry\n");
		return;
	}
	mcu_avdd_proc_entry = proc_create(ZTE_GPS_MCU_AVDD, 0666, gps_proc_dir, &proc_ops_mcu_avdd);
	if (mcu_avdd_proc_entry == NULL) {
		BL_ERR("Failed to create mcu avdd entry\n");
		return;
	}
	mcu_prtrg_proc_entry = proc_create(ZTE_GPS_MCU_PRTRG, 0666, gps_proc_dir, &proc_ops_mcu_prtrg);
	if (mcu_prtrg_proc_entry == NULL) {
		BL_ERR("Failed to create mcu prtrg entry\n");
		return;
	}
	return;
}

static int __init gps_gpio_ctl_init(void) {
	GPS_DEBUG("enter \n");
	gps_gpio_init();
	create_gps_proc_entry();
	//reset_chip();
	GPS_DEBUG("done \n");
	return 0;
}

static void __exit gps_gpio_ctl_exit(void) {
	if(gps_proc_dir == NULL) {
		BL_ERR("proc/bl_gpios is null\n");
		return;
	}
	remove_proc_entry(ZTE_GPS_MCU_RESET, gps_proc_dir);
	remove_proc_entry(ZTE_GPS_MCU_AVDD, gps_proc_dir);
	remove_proc_entry(ZTE_GPS_MCU_PRTRG, gps_proc_dir);
	remove_proc_entry(ZTE_GPS_PROC_DIR, NULL);
}

module_init(gps_gpio_ctl_init);
module_exit(gps_gpio_ctl_exit);

MODULE_AUTHOR("zte light");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("zte gps gpio ctl");