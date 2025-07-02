/*
 * zte_pv_pintest.c
 *
 * ZTE pintest control policy and interface for usrspace
 *
 * Copyright (C) 2022        ZTE Inc.
 *
 * Author:      Sikoujun <tian.jun5@zte.com.cn>
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/bitops.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <linux/version.h>
#include <dt-bindings/zte_pintest/pintest.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#define ZTE_PINTEST_NAME  "zte_pintest"

#define DEFAULT_PINTEST_SIZE    (2)
#define INVALID_PINTEST_GPIO_STATE (0xFF)

#define CONNECT_PINS  0
#define DISCONNECT_PINS  (-1)

struct zte_pintest_policy {
	u8 output_gpio_level;
	u8 input_gpio_level;
};

struct zte_pintest_info {
	struct device *parent;
	struct list_head next;
	struct device *dev;
	u32 *data;
	const char *name;
	int output_gpio;
	int input_gpio;
	u8 policy_nums;
	struct zte_pintest_policy *policy;
};

static LIST_HEAD(pintest_list);

static int zte_pintest_dev_major = 0;
static struct class *zte_pintest_ctrl_class;

struct zte_pintest_ctrl_dev {
	struct platform_device  *pdev;
	struct device *dev;
};

struct zte_pintest_ctrl_dev *zte_pintestd = NULL;
/*
static void zte_pintest_set_gpio(int gpio, u8 level)
{
	if(gpio >= 0 ) {
		gpio_set_value(gpio, level);
	}
}
*/
static u8 zte_pintest_get_gpio(int gpio)
{
	return (gpio >= 0) ? gpio_get_value(gpio) : INVALID_PINTEST_GPIO_STATE;
}

static int zte_pinconnect_func(int output_gpio, int input_gpio, u8 output_gpio_level, u8 input_gpio_level)
{
	int ret = 0;
	u8 real_output_gpio_level;
	u8 real_input_gpio_level;

	if( output_gpio < 0 || input_gpio < 0){
		printk("ZTE: para error, and test can not go\n");
		return DISCONNECT_PINS;
	}

	if(output_gpio >= 0) {
		gpio_free(output_gpio);
		ret = gpio_request(output_gpio, NULL);
		if (ret < 0) {
			printk("output_gpio %d requested faild", output_gpio);
			ret = DISCONNECT_PINS;
			goto error;
		}
		else{
			ret = gpio_direction_output(output_gpio, output_gpio_level);
			if (ret < 0) {
				pr_err("%s:gpio_direction_output failed!\n", __func__);
			}
		}
	}

	if(input_gpio >= 0) {
		gpio_free(input_gpio);
		ret = gpio_request(input_gpio, NULL);
		if (ret < 0) {
			printk("input_gpio %d requested faild", input_gpio);
			ret = DISCONNECT_PINS;
			goto error;
		}
		else{
			ret = gpio_direction_input(input_gpio);
			if (ret < 0) {
				pr_err("%s:gpio_direction_input failed!\n", __func__);
			}
		}
	}

	/* wait for some times for gpio state stable */
	usleep_range(100000, 200000);

	/* get real gpio state */
	real_output_gpio_level = zte_pintest_get_gpio(output_gpio);
	real_input_gpio_level = zte_pintest_get_gpio(input_gpio);

	/* compare gpio state with policy */
	if((output_gpio_level == real_output_gpio_level)
		&&(input_gpio_level == real_input_gpio_level)){
		ret = CONNECT_PINS;
		printk("ZTE: GPIO_OUT %d and input_gpio %d test pass\n",output_gpio, input_gpio);
		printk("ZTE: GPIO_OUT %d and real_output_gpio_level is %d\n",output_gpio, real_output_gpio_level);
		printk("ZTE: GPIO_IN %d and real_input_gpio_level is %d\n",input_gpio, real_input_gpio_level);
	}
	else{
		printk("ZTE: GPIO_OUT %d and input_gpio %d test failed\n",output_gpio, input_gpio);
		printk("ZTE: GPIO_OUT %d and real_output_gpio_level is %d\n",output_gpio, real_output_gpio_level);
		printk("ZTE: GPIO_IN %d and real_input_gpio_level is %d\n",input_gpio, real_input_gpio_level);
		printk("ZTE: GPIO_OUT %d and expect value is %d\n",output_gpio, output_gpio_level);
		printk("ZTE: GPIO_IN %d and expect value is %d\n",input_gpio, input_gpio_level);
		ret = DISCONNECT_PINS;
	}

error:
	gpio_free(output_gpio);
	gpio_free(input_gpio);
	return ret;
}

static ssize_t zte_pintest_show_state(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	struct zte_pintest_info *pintest = dev_get_drvdata(dev);
	struct zte_pintest_policy *policy;
	char *tmp = buf;
	int value;
	int i = 0;

	if(NULL == pintest) {
		pr_info("%s: pintest is NULL\n", __func__);
		value = DISCONNECT_PINS;
	}
	else{
		for (i = 0; i < pintest->policy_nums; i++) {
			policy = &pintest->policy[i];
			pr_info("%s:gpio_info %d, %d, %d, %d\n", __func__, pintest->output_gpio, pintest->input_gpio, policy->output_gpio_level, policy->input_gpio_level);
			if(CONNECT_PINS != zte_pinconnect_func(pintest->output_gpio, pintest->input_gpio, policy->output_gpio_level, policy->input_gpio_level))
			{
				printk("ZTE: policy[%d] test failed\n", i);
				value = DISCONNECT_PINS;
				break;
			}
			value = CONNECT_PINS;
		}
	}

	tmp += sprintf(tmp, "%d\n", value);
	return (int)(tmp-buf);
}

static ssize_t zte_pintest_store_gpio_set(struct device *dev,
								   struct device_attribute *attr, const char *buf, size_t count)
{
	struct zte_pintest_info *pintest = dev_get_drvdata(dev);
	int value, ret;
	//int i = 0;
	//struct zte_pintest_policy *policy;

	if(NULL == pintest) {
		pr_info("%s: pintest is NULL\n", __func__);
		return -ENODEV;
	}

	if (sscanf(buf, "%d", &value) < 1) {
		pr_info("store_state error\n");
		return -EINVAL;
	}

	gpio_free(pintest->output_gpio);

	ret = gpio_request(pintest->output_gpio, "test_gpio_out");
	if (ret < 0) {
		printk("ZTE: output_gpio %d requested faild\n",pintest->output_gpio);
		return -EINVAL;
	}
	else{
		ret = gpio_direction_output(pintest->output_gpio, !!value);
		if (ret < 0) {
				pr_err("%s:gpio_direction_output failed!\n", __func__);
		}
		printk("GPIO%d set to %d\n", pintest->output_gpio, !!value);
	}

	return count;
}

static ssize_t zte_pintest_show_help(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct zte_pintest_info *pintest = dev_get_drvdata(dev);
	struct zte_pintest_policy *policy;
	char *tmp = buf;
	int i = 0;

	if(NULL == pintest) {
		pr_info("%s: pintest is NULL\n", __func__);
		return -ENODEV;
	}

	tmp += sprintf(tmp,"\n******************%s pintest Info******************\n", pintest->name);
	tmp += sprintf(tmp,"output_gpio is %d\r\n", pintest->output_gpio);
	tmp += sprintf(tmp,"input_gpio is %d\r\n", pintest->input_gpio);

	tmp += sprintf(tmp, "\n******************Policy Info******************\n" \
				   "output_gpio_level  input_gpio_level\n");

	for (i = 0; i < pintest->policy_nums; i++) {
		policy = &pintest->policy[i];

		tmp += sprintf(tmp, "        %x              %x\n",
					   policy->output_gpio_level,
					   policy->input_gpio_level);
	}
	tmp += sprintf(tmp,"\r\n");

	return (int)(tmp-buf);
}

static ssize_t zte_pintest_null_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t zte_pintest_null_store(struct device *dev,
								  struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static struct device_attribute zte_pintest_info_dev_attrs[] = {
	__ATTR(state, 0644, zte_pintest_show_state, zte_pintest_null_store),
	__ATTR(output_gpio_set, 0644, zte_pintest_null_show, zte_pintest_store_gpio_set),
	__ATTR(help, 0644, zte_pintest_show_help, zte_pintest_null_store),
};

/* for zte_thermal thermal_zone attr on /sys/class/zte_pintest */
void zte_pintest_info_dev_init_attrs(struct device *dev)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(zte_pintest_info_dev_attrs); i++) {
		ret = device_create_file(dev, &zte_pintest_info_dev_attrs[i]);
		if (ret) {
			pr_err("Failed to create files for attribute %s\n",
				   zte_pintest_info_dev_attrs[i].attr.name);
		}
	}
}

static int zte_pintest_of_get_property(struct zte_pintest_info *pintest, struct device_node *np)
{
	const char *pintest_name;
	struct zte_pintest_policy *policy;
	//u32 value = 0;
	int i = 0, j = 0, err = 0;
	int info_size = 0, info_num = 0, offset = 0;
	//int ret;

	if (!of_property_read_string(np, "label", &pintest_name)) {
		pintest->name = pintest_name;
	} else {
		dev_err(pintest->parent, "pintest name not found");
		return -1;
	}

	/* get ctrl details */
	info_size = DEFAULT_PINTEST_SIZE;

	/* get gpio info */
	pintest->output_gpio = of_get_named_gpio(np, "output-gpio", 0);
	pintest->input_gpio = of_get_named_gpio(np, "input-gpio", 0);

	pr_info("pintest->output_gpio is %d , pintest->input_gpio is %d\n",
			pintest->output_gpio, pintest->input_gpio);


	/* get pintest-policy info */
	info_num = of_property_count_elems_of_size(np, "pintest-policy",
					sizeof(u32) * info_size);
	if (info_num < 1) {
		dev_err(pintest->parent, "pintest info num error %d", info_num);
		return -2;
	}

	pintest->data = kzalloc(sizeof(u32)*info_size*info_num, GFP_KERNEL);
	if (!pintest->data) {
		dev_err(pintest->parent, "fail to alloc memory for pintest data");
		return -3;
	}

	for (i = 0; i < info_num; i++) {
		for (j = 0; j < info_size; j++) {
			offset = i * info_size + j;
			err = of_property_read_u32_index(np, "pintest-policy",
											 offset, &pintest->data[offset]);
			if (err) {
				dev_err(pintest->parent, "read data prop [%d] err %d", offset, err);
				kfree(pintest->data);
				return -4;
			}
		}
	}

	pintest->policy = kzalloc(sizeof(struct zte_pintest_policy)*info_num, GFP_KERNEL);
	if (!pintest->policy) {
		dev_err(pintest->parent, "fail to alloc memory for pintest info");
		kfree(pintest->data);
		return -5;
	}
	pintest->policy_nums = info_num;

	for (i = 0; i < pintest->policy_nums; i++) {
		policy = &pintest->policy[i];

		offset = i * info_size;
		policy->output_gpio_level = pintest->data[offset];
		policy->input_gpio_level = pintest->data[offset+1];

		pr_info("pintest %s policy[%u]: output_gpio_level %u, input_gpio_level %u\n",
				pintest->name, i, policy->output_gpio_level,
				policy->input_gpio_level);
	}

	//pr_info("pintest_ptr is 0x%x , pintest->policy ptr is 0x%x\n",
	//		pintest, pintest->policy);

	kfree(pintest->data);
	return 0;
}

static void zte_pintest_free_policy(struct zte_pintest_info *pintest)
{
	if (pintest->policy)
		kfree(pintest->policy);
}

static int zte_pintest_ctrl_parse_dt(struct zte_pintest_ctrl_dev *pintest_ctrl)
{
	int rc;
	int i = 0;
	struct device *dev = pintest_ctrl->dev;
	struct device_node *np = pintest_ctrl->dev->of_node;
	struct device_node *child;
	struct zte_pintest_info *pintest;

	if (!np) {
		pr_err("device tree info. missing\n");
		return -EINVAL;
	}

	for_each_available_child_of_node(np, child) {
		dev_err(dev, "find pintest name:%s", child->name);
		pintest = NULL;

		pintest = kzalloc(sizeof(*pintest), GFP_KERNEL);
		if (!pintest) {
			dev_err(dev, "fail to alloc memory zte pintest %s", child->name);
			continue;
		}
		pintest->parent = dev;

		rc = zte_pintest_of_get_property(pintest, child);
		if (rc) {
			dev_err(dev, "find pintest name:%s rc = %d", child->name, rc);
			kfree(pintest);
			continue;
		}

		pintest->dev = device_create(zte_pintest_ctrl_class, NULL,
									 MKDEV(zte_pintest_dev_major, i),
									 pintest,
									 pintest->name);
		i++; /* for next device_creat */
		if (IS_ERR(pintest->dev)) {
			pr_err("%s: Failed to create %s, ret %ld\n", __func__, pintest->name, PTR_ERR(pintest->dev));
			zte_pintest_free_policy(pintest);
			kfree(pintest);
			continue;
		} else {
			dev_set_drvdata(pintest->dev, pintest);
			zte_pintest_info_dev_init_attrs(pintest->dev);
			pr_info("%s: successed to create %s\n", __func__, pintest->name);
		}

		list_add_tail(&pintest->next, &pintest_list);
	}

	return 0;
}

static int zte_pintest_ctrl_probe(struct platform_device *pdev)
{
	struct zte_pintest_ctrl_dev *zte_pintest_ctrl;

	zte_pintest_ctrl = kzalloc(sizeof(*zte_pintest_ctrl), GFP_KERNEL);
	if (!zte_pintest_ctrl) {
		pr_err("Failed to alloc memory for zte_pintest_ctrl\n");
		return -ENOMEM;
	}

	zte_pintestd = zte_pintest_ctrl;

	dev_set_drvdata(&pdev->dev, zte_pintest_ctrl);
	zte_pintest_ctrl->pdev = pdev;
	zte_pintest_ctrl->dev = &pdev->dev;

	pr_info("%s() enter\n", __func__);

	zte_pintest_ctrl_parse_dt(zte_pintest_ctrl);

	pr_info("%s() done\n", __func__);
	return 0;
}

static int zte_pintest_ctrl_remove(struct platform_device *pdev)
{
	struct zte_pintest_ctrl_dev *zte_pintest_ctrl = dev_get_drvdata(&pdev->dev);
	struct zte_pintest_info *pintest;

	list_for_each_entry(pintest, &pintest_list, next) {
		list_del_init(&pintest->next);
		device_destroy(zte_pintest_ctrl_class, pintest->dev->devt);
		zte_pintest_free_policy(pintest);
		kfree(pintest);
	}

	kfree(zte_pintest_ctrl);
	return 0;
}

static const struct file_operations zte_pintest_ctrl_fops = {
	.owner = THIS_MODULE,
	.open = NULL,
	.release = NULL,
	.unlocked_ioctl = NULL,
};


#ifdef CONFIG_OF
static const struct of_device_id zte_pintest_ctrl_of_match[]= {
	{.compatible = "zte,zte_pintest_ctrl"},
	{},
};
#endif

static const struct platform_device_id zte_pintest_ctrl_id[] = {
	{ "zte_pintest_ctrl", 0 },
};

static struct platform_driver zte_pintest_ctrl_driver = {
	.driver = {
		.name = "zte_pintest_ctrl",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = zte_pintest_ctrl_of_match,
#endif
	},
	.probe = zte_pintest_ctrl_probe,
	 .remove = zte_pintest_ctrl_remove,
	  .id_table = zte_pintest_ctrl_id,
   };

static int __init zte_pintest_ctrl_init(void)
{
	int ret = 0;

	zte_pintest_dev_major = register_chrdev(0, ZTE_PINTEST_NAME, &zte_pintest_ctrl_fops);
	if (zte_pintest_dev_major < 0) {
		pr_err("%s : register zte pintest chrdev failed, err %d\n",
			   __func__, zte_pintest_dev_major);
		return -ENODEV;
	}

	zte_pintest_ctrl_class = class_create(THIS_MODULE, ZTE_PINTEST_NAME);
	if (IS_ERR(zte_pintest_ctrl_class)) {
		pr_err("%s : register zte_pintest_ctrl_class failed\n", __func__);
		ret = PTR_ERR(zte_pintest_ctrl_class);
		goto error_class;
	}

	ret = platform_driver_register(&zte_pintest_ctrl_driver);
	if (ret) {
		pr_err("%s : register zte_pintest_ctrl_driver failed\n", __func__);
		goto err_driver;
	}

	pr_info("%s done\n", __func__);
	return ret;

err_driver:
	class_destroy(zte_pintest_ctrl_class);
error_class:
	unregister_chrdev(zte_pintest_dev_major, ZTE_PINTEST_NAME);
	return ret;

}

static void __exit zte_pintest_ctrl_exit(void)
{
	platform_driver_unregister(&zte_pintest_ctrl_driver);
}

module_init(zte_pintest_ctrl_init);
module_exit(zte_pintest_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sikoujun <tian.jun5@zte.com.cn>");
