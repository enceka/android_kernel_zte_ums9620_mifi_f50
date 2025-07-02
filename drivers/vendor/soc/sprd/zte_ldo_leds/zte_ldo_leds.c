#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>

#define BOOT_MODE_MAX_LEN 20

static char boot_mode[BOOT_MODE_MAX_LEN] = "NONE";

struct zte_ldo_leds_data {
	struct regulator *vddcama0;
	struct regulator *vddcama1;
	struct regulator *vddcama2;
	struct device *dev;
};

static int regulator_vddcama0 = 0;
static int regulator_vddcama1 = 0;
static int regulator_vddcama2 = 0;
static int regulator_vddcama0_status = 0;
static int regulator_vddcama1_status = 0;
static int regulator_vddcama2_status = 0;


static int rpc_get_bootmode(void)
{
	struct device_node *np = NULL;
	const char *cmd_line = NULL, *s = NULL;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np) {
		pr_err("%s: find chosen failed\n", __func__);
		return 0;
	}

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0) {
		pr_err("%s: read bootargs failed\n", __func__);
		return 0;
	}

	s = strstr(cmd_line, "androidboot.mode=");
	if (!s) {
		pr_err("%s: find androidboot.mode failed\n", __func__);
		return 0;
	}

	sscanf(s, "androidboot.mode=%s", boot_mode);
	pr_info("%s: androidboot.mode is %s\n", __func__, boot_mode);
	return 0;
}

static ssize_t vddcama0_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%d\n", regulator_vddcama0);
}

static ssize_t vddcama0_status_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct platform_device *pdev = container_of(dev,
						    struct platform_device,
						    dev);
	struct zte_ldo_leds_data *data = platform_get_drvdata(pdev);

	unsigned long vol_mv;
	int ret;

	ret = kstrtoul(buf, 10, &vol_mv);
	if (ret) {
		pr_err("%s: parse vol failed\n", __func__);
		return ret;
	}

	if (vol_mv > 0)
		vol_mv = 1;
	else
		vol_mv = 0;

	if (vol_mv > 0) {
		regulator_set_voltage(data->vddcama0, vol_mv * 3300000, vol_mv * 3300000);
		if (regulator_vddcama0_status == 0) {
			regulator_enable(data->vddcama0);
			regulator_vddcama0_status = 1;
		}
	} else {
		if (regulator_vddcama0_status == 1) {
			regulator_disable(data->vddcama0);
			regulator_vddcama0_status = 0;
		}
	}

	regulator_vddcama0 = vol_mv;

	return count;
}

static ssize_t vddcama1_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%d\n", regulator_vddcama1);
}

static ssize_t vddcama1_status_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct platform_device *pdev = container_of(dev,
						    struct platform_device,
						    dev);
	struct zte_ldo_leds_data *data = platform_get_drvdata(pdev);

	unsigned long vol_mv;
	int ret;

	ret = kstrtoul(buf, 10, &vol_mv);
	if (ret) {
		pr_err("%s: parse vol failed\n", __func__);
		return ret;
	}

	if (vol_mv > 0)
		vol_mv = 1;
	else
		vol_mv = 0;

	if (vol_mv > 0) {
		regulator_set_voltage(data->vddcama1, vol_mv * 3300000, vol_mv * 3300000);
		if (regulator_vddcama1_status == 0) {
			regulator_enable(data->vddcama1);
			regulator_vddcama1_status = 1;
		}
	} else {
		if (regulator_vddcama1_status == 1) {
			regulator_disable(data->vddcama1);
			regulator_vddcama1_status = 0;
		}
	}

	regulator_vddcama1 = vol_mv;

	return count;
}

static ssize_t vddcama2_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%d\n", regulator_vddcama2);
}

static ssize_t vddcama2_status_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct platform_device *pdev = container_of(dev,
						    struct platform_device,
						    dev);
	struct zte_ldo_leds_data *data = platform_get_drvdata(pdev);

	unsigned long vol_mv;
	int ret;

	ret = kstrtoul(buf, 10, &vol_mv);
	if (ret) {
		pr_err("%s: parse vol failed\n", __func__);
		return ret;
	}

	if (vol_mv > 0)
		vol_mv = 1;
	else
		vol_mv = 0;

	if (vol_mv > 0) {
		regulator_set_voltage(data->vddcama2, vol_mv * 3300000, vol_mv * 3300000);
		if (regulator_vddcama2_status == 0) {
			regulator_enable(data->vddcama2);
			regulator_vddcama2_status = 1;
		}
	} else {
		if (regulator_vddcama2_status == 1) {
			regulator_disable(data->vddcama2);
			regulator_vddcama2_status = 0;
		}
	}

	regulator_vddcama2 = vol_mv;

	return count;
}

static DEVICE_ATTR(vddcama0_status, 0664, vddcama0_status_show, vddcama0_status_store);
static DEVICE_ATTR(vddcama1_status, 0664, vddcama1_status_show, vddcama1_status_store);
static DEVICE_ATTR(vddcama2_status, 0664, vddcama2_status_show, vddcama2_status_store);

static struct attribute *zte_ldo_leds_attributes[] = {
	&dev_attr_vddcama0_status.attr,
	&dev_attr_vddcama1_status.attr,
	&dev_attr_vddcama2_status.attr,
	NULL
};

static const struct attribute_group zte_ldo_leds_group = {
	.attrs = zte_ldo_leds_attributes,
};

static int zte_ldo_leds_probe(struct platform_device *pdev)
{
	struct zte_ldo_leds_data *drvdata;
	int ret = 0;
	pr_info("%s enter\n", __func__);

	drvdata =
	    devm_kzalloc(&pdev->dev, sizeof(struct zte_ldo_leds_data), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		drvdata->vddcama0 = regulator_get(&pdev->dev, "vddcama0");
		if (IS_ERR(drvdata->vddcama0)) {
			pr_err("%s: regulator_get vddcama0 failed\n", __func__);
			return -1;
		}

		drvdata->vddcama1 = regulator_get(&pdev->dev, "vddcama1");
		if (IS_ERR(drvdata->vddcama1)) {
			pr_err("%s: regulator_get vddcama1 failed\n", __func__);
			return -1;
		}

		drvdata->vddcama2 = regulator_get(&pdev->dev, "vddcama2");
		if (IS_ERR(drvdata->vddcama2)) {
			pr_err("%s: regulator_get vddcama2 failed\n", __func__);
			return -1;
		}

		drvdata->dev = &pdev->dev;

		ret = sysfs_create_group(&pdev->dev.kobj, &zte_ldo_leds_group);
		if (ret) {
			pr_err("%s: sysfs create group failed\n", __func__);
			return -1;
		}

		platform_set_drvdata(pdev, drvdata);

	    rpc_get_bootmode();
	    if (strcmp(boot_mode, "charger") == 0) {
	        pr_info("%s: in charger mode, disable ldo leds!!\n", __func__);
			regulator_disable(drvdata->vddcama0);
			regulator_disable(drvdata->vddcama1);
			regulator_disable(drvdata->vddcama2);
        }
	}

	pr_info("%s exit\n", __func__);
	return 0;
}

static int zte_ldo_leds_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id of_zte_ldo_leds_match[] = {
	{.compatible = "zte-ldo-leds",},
	{},
};

static struct platform_driver zte_ldo_leds_driver = {
	.probe = zte_ldo_leds_probe,
	.remove = zte_ldo_leds_remove,
	.driver = {
		   .name = "zte-ldo-leds",
		   .of_match_table = of_zte_ldo_leds_match,
		   },
};

module_platform_driver(zte_ldo_leds_driver);

MODULE_DESCRIPTION("ZTE LDO LEDS driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:zte-ldo-leds");
