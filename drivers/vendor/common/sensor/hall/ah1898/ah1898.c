/***********************************
*****hall sensor****************
************************************
************************************/
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#define HALL_SENSOR_DOWN "hall_event_down=true"
#define HALL_SENSOR_UP "hall_event_up=true"

#define WAKELOCK_HOLD_TIME 1000 /* in ms*/

static int hall_status = 1;
struct pinctrl *hall_gpio_pinctrl = NULL;
struct pinctrl_state *hall_gpio_state = NULL;
module_param(hall_status, int, 0644);

struct hall_chip {
	/*struct mutex lock;*/
	struct input_dev *input;
	struct work_struct work;
	struct regulator *vddio;
	struct platform_device *pdev;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
	struct wakeup_source *hall_wakelock;
#else
	struct wake_lock hall_wakelock;
#endif
	u32 min_uv;	/* device allow minimum voltage */
	u32 max_uv;	/* device allow max voltage */
	int irq;
	bool hall_enabled;
} *hall_chip_data;

static inline void report_uevent(struct hall_chip *hall_chip_data, char *str)
{
	char *envp[2];
	envp[0] = str;
	envp[1] = NULL;
	kobject_uevent_env(&(hall_chip_data->pdev->dev.kobj), KOBJ_CHANGE, envp);
}

static void hall_work_func(struct work_struct *work)
{
	int value;
	if (hall_chip_data->input == NULL || hall_chip_data == NULL) {
		pr_info("hall_work_fuc ERROR");
		return;
	}

	value = gpio_get_value(hall_chip_data->irq);
	pr_info("%s:hall test value=%d, enabled = %d\n", __func__, value, hall_chip_data->hall_enabled);

	if (hall_chip_data->hall_enabled == 1) {
		if (value == 1) {
			pr_info("%s:hall ===switch is off!!the value = %d\n", __func__, value);
			/* delete KEY_POWER input report
			input_report_key(hall_chip_data->input, KEY_POWER, 1);
			input_sync(hall_chip_data->input);
			input_report_key(hall_chip_data->input, KEY_POWER, 0);*/
			report_uevent(hall_chip_data, HALL_SENSOR_UP);
			hall_status = 1;
		} else {
			pr_info("%s:hall ===switch is on!!the value = %d\n", __func__, value);
			/* delete KEY_POWER input report
			input_report_key(hall_chip_data->input, KEY_POWER, 1);
			input_sync(hall_chip_data->input);
			input_report_key(hall_chip_data->input, KEY_POWER, 0);*/
			report_uevent(hall_chip_data, HALL_SENSOR_DOWN);
			hall_status = 0;
		}
	}

	/*enable_irq(hall_chip_data->irq);*/
}

static irqreturn_t hall_interrupt(int irq, void *dev_id)
{
	if (hall_chip_data == NULL) {
       /* printk("++++++++hall_interrupt\n ddata = %x", (unsigned long)hall_chip_data);*/
       return IRQ_NONE;
	}
	pr_info("%s:chenhui hall_interrupt!!\n", __func__);
	/*disable_irq_nosync(irq);*/
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
	__pm_wakeup_event(hall_chip_data->hall_wakelock, WAKELOCK_HOLD_TIME);
#else
	wake_lock_timeout(&hall_chip_data->hall_wakelock, msecs_to_jiffies(WAKELOCK_HOLD_TIME));
#endif

	schedule_work(&hall_chip_data->work);
	return IRQ_HANDLED;
}

static int hall_parse_dt(struct platform_device *pdev)
{
	u32 tempval;
	int rc;
	rc = of_property_read_u32(pdev->dev.of_node, "linux,max-uv", &tempval);
	if (rc) {
		dev_err(&pdev->dev, "unable to read max-uv\n");
		return -EINVAL;
	}

	hall_chip_data->max_uv = tempval;
	rc = of_property_read_u32(pdev->dev.of_node, "linux,min-uv", &tempval);
	if (rc) {
		dev_err(&pdev->dev, "unable to read min-uv\n");
		return -EINVAL;
	}
	hall_chip_data->min_uv = tempval;

	hall_chip_data->irq = of_get_named_gpio(pdev->dev.of_node, "ah,gpio_irq", 0);

	if (!gpio_is_valid(hall_chip_data->irq)) {
		pr_info("gpio irq pin %d is invalid.\n", hall_chip_data->irq);
		return -EINVAL;
	}

	return 0;
}

static int hall_config_regulator(struct platform_device *dev, bool on)
{
	int rc = 0;

	if (on) {
		hall_chip_data->vddio = devm_regulator_get(&dev->dev, "vddio");
		if (IS_ERR(hall_chip_data->vddio)) {
			rc = PTR_ERR(hall_chip_data->vddio);
			dev_err(&dev->dev, "Regulator vddio get failed rc=%d\n", rc);
			hall_chip_data->vddio = NULL;
			return rc;
		}

		if (regulator_count_voltages(hall_chip_data->vddio) > 0) {
			rc = regulator_set_voltage(
					hall_chip_data->vddio,
					hall_chip_data->min_uv,
					hall_chip_data->max_uv);
			if (rc) {
				dev_err(&dev->dev, "Regulator vddio Set voltage failed rc=%d\n", rc);
				goto deinit_vregs;
			}
		}
		return rc;
	}

	goto deinit_vregs;

deinit_vregs:
	if (regulator_count_voltages(hall_chip_data->vddio) > 0)
		regulator_set_voltage(hall_chip_data->vddio, 0, hall_chip_data->max_uv);
	return rc;
}

static int hall_set_regulator(struct platform_device *dev, bool on)
{
	int rc = 0;
	if (on) {
		if (!IS_ERR_OR_NULL(hall_chip_data->vddio)) {
			rc = regulator_enable(hall_chip_data->vddio);
			if (rc) {
				dev_err(&dev->dev, "Enable regulator vddio failed rc=%d\n", rc);
				goto disable_regulator;
			}
		}
		return rc;
	}

	if (!IS_ERR_OR_NULL(hall_chip_data->vddio)) {
		rc = regulator_disable(hall_chip_data->vddio);
		if (rc)
			dev_err(&dev->dev, "Disable regulator vddio failed rc=%d\n", rc);
	}
	return 0;

disable_regulator:
	if (!IS_ERR_OR_NULL(hall_chip_data->vddio))
		regulator_disable(hall_chip_data->vddio);
	return rc;
}

static int  hall_probe(struct platform_device *pdev)
{
	int value_status;
	int error = 0;
	int irq = 0;
	char *hall_irq_name = kstrdup("hall_irq", GFP_KERNEL);
	pr_info("++++++++hall_probe\n");

	if (pdev->dev.of_node == NULL) {
		dev_info(&pdev->dev, "can not find device tree node\n");
		error = -ENODEV;
		goto exit;
	}

	hall_chip_data = kzalloc(sizeof(struct hall_chip), GFP_KERNEL);
	if (!hall_chip_data) {
		error = -ENOMEM;
		goto exit;
	}

	hall_chip_data->pdev = pdev;

	hall_chip_data->input = input_allocate_device();
	if (!hall_chip_data->input) {
		error = -ENOMEM;
		goto fail0;
	}

	hall_chip_data->input->name = "hall";
	set_bit(EV_KEY, hall_chip_data->input->evbit);
	error = input_register_device(hall_chip_data->input);
	if (error) {
		pr_err("hall: Unable to register input device, error: %d\n", error);
		goto fail0;
	}

	if (pdev->dev.of_node) {
		error = hall_parse_dt(pdev);
		if (error < 0) {
			dev_err(&pdev->dev, "Failed to parse device tree\n");
			goto fail1;
		}
	} else {
		dev_err(&pdev->dev, "No valid platform data.\n");
		error = -ENODEV;
		goto fail1;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
	hall_chip_data->hall_wakelock = wakeup_source_register(NULL, "hall_wakelock");
#else
	wake_lock_init(&hall_chip_data->hall_wakelock, WAKE_LOCK_SUSPEND, "hall_wakelock");
#endif

	if (hall_chip_data->irq) {
		irq = gpio_to_irq(hall_chip_data->irq);
		error = gpio_request(hall_chip_data->irq, hall_irq_name);
		if (error) {
			pr_info("%s:hall error3\n", __func__);
			goto fail2;
		}
		error = gpio_direction_input(hall_chip_data->irq);
		if (error) {
			pr_info("%s:hall error3\n", __func__);
			goto fail3;
		}
	}

	if (irq) {
		INIT_WORK(&(hall_chip_data->work), hall_work_func);
		error = request_threaded_irq(irq, NULL, hall_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"hall_irq", NULL);
		if (error) {
			pr_err("gpio-hall: Unable to claim irq %d; error %d\n", irq, error);
			goto fail3;
		}

		enable_irq_wake(irq);
	}

	hall_chip_data->hall_enabled = 1;

	value_status = gpio_get_value(hall_chip_data->irq);
	pr_err("gpio-hall: irq %d;\n", value_status);
	if (value_status == 1) {
		hall_status = 1;
	} else {
		hall_status = 0;
	}

	error = hall_config_regulator(pdev, true);
	if (error < 0) {
		dev_err(&pdev->dev, "Configure power failed: %d\n", error);
		goto free_irq;
	}

	error = hall_set_regulator(pdev, true);
	if (error < 0) {
		dev_err(&pdev->dev, "power on failed: %d\n", error);
		goto err_regulator_init;
	}

	pr_info("hall Init hall_state=%d\n", hall_status);
	pr_info("%s:hall Init success!\n", __func__);
	return 0;

err_regulator_init:
	hall_config_regulator(pdev, false);
free_irq:
	disable_irq_wake(irq);
fail3:
	gpio_free(hall_chip_data->irq);
fail2:
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
	wakeup_source_unregister(hall_chip_data->hall_wakelock);
#else
	wake_lock_destroy(&hall_chip_data->hall_wakelock);
#endif
fail1:
	input_unregister_device(hall_chip_data->input);
fail0:
	platform_set_drvdata(pdev, NULL);
	kfree(hall_chip_data);
exit:
	return error;
}

static int hall_remove(struct platform_device *pdev)
{
	gpio_free(hall_chip_data->irq);
	input_unregister_device(hall_chip_data->input);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0))
	wakeup_source_unregister(hall_chip_data->hall_wakelock);
#else
	wake_lock_destroy(&hall_chip_data->hall_wakelock);
#endif
	kfree(hall_chip_data);
	return 0;
}

static struct of_device_id hall_hall_of_match[] = {
	{.compatible = "ah,hall_ic", },
	{},
};

static struct platform_driver hall_hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
		.name = "hall_ic",
		.owner = THIS_MODULE,
		.of_match_table = hall_hall_of_match,
		/*.pm = &led_pm_ops,*/
	},
};

static int  hall_init(void)
{
	pr_info("++++++++hall_init\n");
	return platform_driver_register(&hall_hall_driver);
}

static void  hall_exit(void)
{
	platform_driver_unregister(&hall_hall_driver);
	pr_info("++++++++hall_exit\n");
}

module_init(hall_init);
module_exit(hall_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("libixuan");
MODULE_DESCRIPTION("hall sensor driver");
MODULE_ALIAS("platform:hall-sensor");
