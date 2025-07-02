
/***********************
 * file : ztp_state_change.c
 ***********************/

#include "ztp_common.h"
#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
#include "ztp_ufp.h"
#endif

static char *lcdstate_to_str[] = {
	"screen_on",
	"screen_off",
	"screen_in_doze",
};

static char *lcdchange_to_str[] = {
	"lcd_exit_lp",
	"lcd_enter_lp",
	"lcd_on",
	"lcd_off",
};
struct notifier_block tpd_nb;
atomic_t current_lcd_state = ATOMIC_INIT(SCREEN_ON);
atomic_t current_psensor_state = ATOMIC_INIT(DISABLE_PSENSOR);

#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
extern struct ufp_ops ufp_tp_ops;
#endif
DEFINE_MUTEX(ufp_mac_mutex);

static inline void lcd_on_thing(void)
{
	struct ztp_device *cdev = tpd_cdev;
	int tp_time = 0;
	int ret = 0;

	tp_time = get_tp_consum_time(cdev->ztp_time.lcd_power_on_time);
	TPD_DMESG("tp_time lcd power on -> tp resume start:%d.", tp_time);
	if (cdev->ztp_time.tp_double_tap_time) {
		tp_time = get_tp_consum_time(cdev->ztp_time.tp_double_tap_time);
		TPD_DMESG("tp_time double tap -> tp resume:%d.", tp_time);
		cdev->ztp_time.tp_double_tap_time = 0;
	}
	cdev->tp_suspend_write_gesture = false;
	if (cdev->ztp_pm_suspend) {
		ret = wait_for_completion_timeout(&cdev->ztp_pm_completion, msecs_to_jiffies(700));
		if (!ret) {
			TPD_DMESG("Warning:still in pm_suspend(deep) and has timeout 700ms");
		}
	}
	queue_work(cdev->tpd_wq, &(cdev->resume_work));
}

static inline void lcd_off_thing(void)
{
	struct ztp_device *cdev = tpd_cdev;

#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
	reinit_completion(&ufp_tp_ops.ufp_completion);
#endif
	queue_work(cdev->tpd_wq, &(cdev->suspend_work));
}

static inline void lcd_doze_thing(void)
{
	struct ztp_device *cdev = tpd_cdev;

	queue_work(cdev->tpd_wq, 	&(cdev->suspend_work));
}

static void screen_on(lcdchange lcd_change)
{
	switch (lcd_change) {
	case ENTER_LP:
		atomic_set(&current_lcd_state, DOZE);
		lcd_doze_thing();
		break;
	case LCD_OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		lcd_off_thing();
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

static void screen_off(lcdchange lcd_change)
{
	switch (lcd_change) {
	case LCD_ON:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		break;
	case ENTER_LP:
		atomic_set(&current_lcd_state, DOZE);
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

static void doze(lcdchange lcd_change)
{
	switch (lcd_change) {
	case EXIT_LP:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		break;
	case LCD_OFF:
		atomic_set(&current_lcd_state, SCREEN_OFF);
		break;
	case LCD_ON:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		break;
	default:
		UFP_ERR("ignore err lcd change");
	}
}

void change_tp_state(lcdchange lcd_change)
{
	struct ztp_device *cdev = tpd_cdev;

	mutex_lock(&cdev->tp_resume_mutex);

	UFP_INFO("current_lcd_state:%s, lcd change:%s\n",
			lcdstate_to_str[atomic_read(&current_lcd_state)],
							lcdchange_to_str[lcd_change]);
	switch (atomic_read(&current_lcd_state)) {
	case SCREEN_ON:
		screen_on(lcd_change);
		break;
	case SCREEN_OFF:
		screen_off(lcd_change);
		break;
	case DOZE:
		doze(lcd_change);
		break;
	default:
		atomic_set(&current_lcd_state, SCREEN_ON);
		lcd_on_thing();
		UFP_ERR("err lcd light change");
	}

	mutex_unlock(&cdev->tp_resume_mutex);
}

void change_psensor_state(psensor_state psensor_state)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev == NULL)
	{
		UFP_ERR("err: cdev is NULL");
		return;
	}

	mutex_lock(&cdev->tp_resume_mutex);
	if (atomic_read(&current_psensor_state) == psensor_state)
	{
		UFP_ERR("err: current_psensor_state equal change_state");
		mutex_unlock(&cdev->tp_resume_mutex);
		return;
	}
	switch (psensor_state) {
	case DISABLE_PSENSOR:
		cdev->is_psensor_enable = false;
		atomic_set(&current_psensor_state, DISABLE_PSENSOR);
		break;
	case ENABLE_PSENSOR:
		cdev->is_psensor_enable = true;
		atomic_set(&current_psensor_state, ENABLE_PSENSOR);
		break;
	case PSENSOR_BEGIN_SUSPEND:
		tpd_notifier_call_chain(PSENSOR_NOTIFY_LCD_SUSPEND);
		atomic_set(&current_psensor_state, PSENSOR_BEGIN_SUSPEND);
		break;
	case PSENSOR_BEGIN_RESUME:
		tpd_notifier_call_chain(PSENSOR_NOTIFY_LCD_RESUME);
		atomic_set(&current_psensor_state, PSENSOR_BEGIN_RESUME);
		break;
	default:
		atomic_set(&current_psensor_state, DISABLE_PSENSOR);
		cdev->is_psensor_enable = false;
		UFP_ERR("err psensor_state change");
	}

	mutex_unlock(&cdev->tp_resume_mutex);
}

static void tpd_resume_work(struct work_struct *work)
{
	struct ztp_device *cdev = tpd_cdev;
	int tp_time = 0;

	if (cdev->tp_resume_func) {		
		cdev->ztp_time.tp_reume_start_time = jiffies;
		cdev->tp_resume_func(cdev->tp_data);
		tp_time = get_tp_consum_time(cdev->ztp_time.tp_reume_start_time);
		TPD_DMESG("tp_time tp resume start -> tp resume end:%d.", tp_time);
	}
}

static void tpd_suspend_work(struct work_struct *work)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tp_suspend_func)
		cdev->tp_suspend_func(cdev->tp_data);
}

int suspend_tp_need_awake(void)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tpd_suspend_need_awake) {
		return cdev->tpd_suspend_need_awake(cdev);
	}
	return 0;
}

bool tp_esd_check(void)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tpd_esd_check) {
		return cdev->tpd_esd_check(cdev);
	}
	return 0;
}

void set_lcd_reset_processing(bool enable)
{
	struct ztp_device *cdev = tpd_cdev;

	if (enable) {
		cdev->ignore_tp_irq = true;
	} else {
		cdev->ignore_tp_irq = false;
	}
	TPD_DMESG("cdev->ignore_tp_irq is %d.\n", cdev->ignore_tp_irq);
}

void enable_tpd_irq(bool value)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tpd_enable_irq) {
		TPD_DMESG("tpd_enable_irq %d\n", value);
		cdev->tpd_enable_irq(value);
	}
}

int set_gpio_mode(u8 mode)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->set_gpio_mode) {
		return cdev->set_gpio_mode(cdev, mode);
	}
	return -EIO;
}

void tpd_reset_gpio_output(bool value)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tp_reset_gpio_output) {
		cdev->tp_reset_gpio_output(value);
	}
}

#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
extern void ufp_report_lcd_state(void);

static void ufp_report_lcd_state_work(struct work_struct *work)
{
	ufp_report_lcd_state();
}

void ufp_report_lcd_state_delayed_work(u32 ms)
{
	struct ztp_device *cdev = tpd_cdev;

	mod_delayed_work(cdev->tpd_wq, &cdev->tpd_report_lcd_state_work, msecs_to_jiffies(ms));

}

void cancel_report_lcd_state_delayed_work(void)
{
	struct ztp_device *cdev = tpd_cdev;

	cancel_delayed_work_sync(&cdev->tpd_report_lcd_state_work);

}
#endif

void tpd_resume_work_init(void)
{
	struct ztp_device *cdev = tpd_cdev;

	TPD_DMESG("enter");
	INIT_WORK(&cdev->resume_work, tpd_resume_work);
	INIT_WORK(&cdev->suspend_work,tpd_suspend_work);
#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
	INIT_DELAYED_WORK(&cdev->tpd_report_lcd_state_work, ufp_report_lcd_state_work);
#endif

}

void tpd_resume_work_deinit(void)
{
	struct ztp_device *cdev = tpd_cdev;

	TPD_DMESG("enter");
	cancel_work_sync(&cdev->resume_work);
	cancel_work_sync(&cdev->suspend_work);
#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
	cancel_delayed_work_sync(&cdev->tpd_report_lcd_state_work);
#endif

}

int get_tp_consum_time(unsigned long jiffies_time)
{
	int tp_time = 0;

	tp_time = jiffies_to_msecs(jiffies - jiffies_time);
	return tp_time;
}

#ifdef CONFIG_TOUCHSCREEN_LCD_NOTIFY
static int tpd_lcd_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct ztp_device *cdev = tpd_cdev;
	int tp_time = 0;
	int ret = 0;

	if (tpd_cdev == NULL) {
		TPD_DMESG("zte touch deinit, return\n");
		return -ENOMEM;
	}
	switch (event) {
	case LCD_POWER_ON:
		cdev->ztp_time.lcd_power_on_time = jiffies;
		TPD_DMESG("lcd power on\n");
		set_lcd_reset_processing(true);
		tpd_reset_gpio_output(1);
		enable_tpd_irq(false);
		break;
	case LCD_RESET:
		tp_time = get_tp_consum_time(cdev->ztp_time.lcd_power_on_time);
		TPD_DMESG("lcd reset, tp_time tp power on -> lcd reset time:%d\n", tp_time);
		if (cdev->tp_resume_before_lcd_cmd)
			 change_tp_state(LCD_ON);
		break;
	case LCD_CMD_ON:
		TPD_DMESG("lcd cmd on\n");
		if (!cdev->tp_resume_before_lcd_cmd)
			change_tp_state(LCD_ON);
		set_lcd_reset_processing(false);
		enable_tpd_irq(true);
		break;
	case LCD_CMD_OFF:
		TPD_DMESG("lcd cmd off\n");
		if (cdev->need_tp_resume) {
			TPD_DMESG("tp event processing, need wait");
			ret = wait_for_completion_timeout(&cdev->tp_event_completion, msecs_to_jiffies(2000));
			if (!ret) {
				TPD_DMESG("Warning:wait tp_event_completion timeout 2000ms");
			}
		}
		if (suspend_tp_need_awake()) {
			tpd_notifier_call_chain(LCD_SUSPEND_POWER_ON);
		} else {
			tpd_notifier_call_chain(LCD_SUSPEND_POWER_OFF);
			set_lcd_reset_processing(true);
			enable_tpd_irq(false);
		}
		if (!cdev->tp_suspend_after_lcd_cmd_off_end)
			change_tp_state(LCD_OFF);
		break;
	case LCD_CMD_OFF_END:
		TPD_DMESG("lcd cmd off end\n");
		if (cdev->tp_suspend_after_lcd_cmd_off_end) {
			change_tp_state(LCD_OFF);
			usleep_range(5000, 6000);
		}
		break;
	case LCD_POWER_OFF:
		TPD_DMESG("lcd power off\n");
		break;
	case LCD_POWER_OFF_RESET_LOW:
		TPD_DMESG("lcd power off reset low\n");
		 tpd_reset_gpio_output(0);
		break;
	case LCD_ENTER_AOD:
		if (cdev->ztp_time.tp_single_tap_time) {
			tp_time = get_tp_consum_time(cdev->ztp_time.tp_single_tap_time);
			TPD_DMESG("tp_time single tap -> tp enter aod:%d.", tp_time);
			cdev->ztp_time.tp_single_tap_time = 0;
		}
		TPD_DMESG("lcd enter aod\n");
#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
		ufp_notifier_cb(true);
		ufp_report_lcd_state_delayed_work(50);
#endif
		break;
case LCD_EXIT_AOD:
		TPD_DMESG("lcd exit aod\n");
#ifdef CONFIG_TOUCHSCREEN_UFP_MAC
		ufp_notifier_cb(false);
#endif
		break;
	default:
		TPD_DMESG("lcd state unknown\n");
		break;
	}
	return 0;
}

void lcd_notify_register(void)
{
	int ret = 0;

	tpd_nb.notifier_call = tpd_lcd_notifier_callback;
	ret = lcd_notifier_register_client(&tpd_nb);
	if (ret) {
		TPD_DMESG(" Unable to register fb_notifier: %d\n", ret);
	}
	TPD_DMESG(" register lcd notifier success\n");
}
void lcd_notify_unregister(void)
{
	lcd_notifier_unregister_client(&tpd_nb);
}
#endif

