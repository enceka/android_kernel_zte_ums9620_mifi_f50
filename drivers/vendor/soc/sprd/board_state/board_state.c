#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h> /* misc_register */
#include <linux/fs.h> /* struct file_operations */
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/of.h>

MODULE_LICENSE("Dual BSD/GPL");

#define ZTE_BSP_LOG_PREFIX "[ZTE_LDD_BOOT][BOARDSTATE]"
#define log_err(fmt, ...) pr_err(ZTE_BSP_LOG_PREFIX "[ERR]" fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...) pr_warn(ZTE_BSP_LOG_PREFIX "[WARN]" fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) pr_info(ZTE_BSP_LOG_PREFIX "[INFO]" fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug(ZTE_BSP_LOG_PREFIX "[DEBUG]" fmt, ##__VA_ARGS__)

#define MAX_SIZE 50
#define EFUSE_ENABLE_VALUE 5
#define HARDWAREID_PROC_NAME "driver/board_id"
#define BOARDID_CMDLINE_STR "androidboot.zte_boardid="
#define EFUSE_STATE_CMDLINE_STR "efuse_state="

static int boardid = -1;
static int efuse_state = -1;

static int __init boardid_setup(const char *cmdline)
{
	const char *p;
	char buf[16];
	int i = 0;
	long value;
	int err;

	p = strstr(cmdline, BOARDID_CMDLINE_STR);
	if (!p)
		return -1;

	p = p + strlen(BOARDID_CMDLINE_STR);
	while (isspace(*p)) {
		if (*p == '\0')
			return -1;
		p++;
	}

	memset(buf, 0, 16);
	for (i = 0; i < 16; i++) {
		if ((!isspace(*p)) && *p)
			buf[i] = *p;
		else {
			buf[i] = '\0';
			break;
		}
		p++;
	}

	err = kstrtol(buf, 0, &value);
	if (err)
		return err;
	boardid = value;

	return 0;
}

int zte_get_boardid(void)
{
	return boardid;
}

EXPORT_SYMBOL(zte_get_boardid);

static int __init efuse_state_setup(const char *cmdline)
{
	const char *p;
	char buf[16];
	int i = 0;
	long value;
	int err;

	p = strstr(cmdline, EFUSE_STATE_CMDLINE_STR);
	if (!p)
		return -1;
	p = p + strlen(EFUSE_STATE_CMDLINE_STR);
	while (isspace(*p)) {
		if (*p == '\0')
			return -1;
		p++;
	}

	memset(buf, 0, 16);
	for (i = 0; i < 16; i++) {
		if ((!isspace(*p)) && *p)
			buf[i] = *p;
		else {
			buf[i] = '\0';
			break;
		}
		p++;
	}

	err = kstrtol(buf, 0, &value);
	if (err)
		return err;
	if (value == EFUSE_ENABLE_VALUE)
		efuse_state = 1;
	else
		efuse_state = 0;

	return 0;
}

int zte_get_efuse_state(void)
{
	return efuse_state;
}
EXPORT_SYMBOL(zte_get_efuse_state);

static ssize_t zte_secstate_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", efuse_state);
}

static DEVICE_ATTR(secstate, S_IRUGO, zte_secstate_show, NULL);

static struct attribute *mvd_attrs[] = {
	&dev_attr_secstate.attr,
	NULL,
};

static struct attribute_group mvd_attr_group = {
	.attrs = mvd_attrs,
};

static int zte_board_state_misc_open(struct inode *inode, struct file *file)
{
	log_debug("open\n");
	return 0;
}

static long zte_board_state_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	log_debug("ioctl\n");

	return 0;
}

static struct file_operations zte_board_state_misc_fops = {
	.owner = THIS_MODULE,
	.open = zte_board_state_misc_open,
	.unlocked_ioctl = zte_board_state_misc_ioctl,
};

static struct miscdevice zte_board_state_misc_dev[] = {
	{
		.minor    = MISC_DYNAMIC_MINOR,
		.name    = "zte_board_state",
		.fops    = &zte_board_state_misc_fops,
	}
};

static int hardwareid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", boardid);
	return 0;
}

static int hardwareid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hardwareid_proc_show, NULL);
}

struct proc_dir_entry *hardwareid_proc_dir = NULL;

static struct file_operations hardwareid_proc_fops = {
	.owner	= THIS_MODULE,
	.open	= hardwareid_proc_open,
	.read	= seq_read,
	.llseek	= seq_lseek,
	.release = single_release,
};

static int __init zte_board_state_init(void)
{
	int ret = 0;
	struct proc_dir_entry *file;
	struct device_node *cmdline_node;
	const char *cmdline;

	log_info("zte_board state init\n");

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		log_err("%s: Can't not parse bootargs\n", __func__);
		return -1;
	}

	boardid_setup(cmdline);
	efuse_state_setup(cmdline);

	ret = misc_register(&zte_board_state_misc_dev[0]);
	if (ret) {
		log_err("fail to register misc driver: %d\n", ret);
		goto register_fail;
	}

	/*ret = device_create_file(zte_board_state_misc_dev[0].this_device, &dev_attr_id);*/
	ret  = sysfs_create_group(&zte_board_state_misc_dev[0].this_device->kobj, &mvd_attr_group);
	if (ret) {
		log_err("fail to create file: %d\n", ret);
		goto register_fail;
	}
	ret = zte_get_efuse_state();
	log_debug("zte_get_efuse_state(%d)\n", ret);

	file = NULL;

	file = proc_create(HARDWAREID_PROC_NAME, 0644, NULL, &hardwareid_proc_fops);
	if (!file) {
        printk("%s proc_create failed!\n", __func__);
	    return -ENOMEM;
    }
	
	return 0;

register_fail:
	return ret;
}

static void __exit zte_board_state_exit(void)
{
	log_info("zte_board_state_exit exit\n");
	sysfs_remove_group(&zte_board_state_misc_dev[0].this_device->kobj, &mvd_attr_group);
	misc_deregister(&zte_board_state_misc_dev[0]);
}

module_init(zte_board_state_init);
module_exit(zte_board_state_exit);
