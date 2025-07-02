/*
 * AXS touchscreen driver.
 *
 * Copyright (c) 2020-2021 AiXieSheng Technology. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "axs_core.h"

#if AXS_ESD_CHECK_EN
#define ESD_CHECK_DELAY_TIME               500 /*unit:ms*/

/*bool axs_esd_error(int exception_code)
{
    bool ret = false;
    switch (exception_code)
    {
        case RAWDATA_EXCEPTION:
        case SCAN_EXCEPTION:
        case ESD_EXCEPTION:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
} */
void axs_esd_reset_process(void)
{
	AXS_INFO("axs_esd_reset_process!\n");
/*
#if AXS_DOWNLOAD_APP_EN
	if (!axs_download_init())
	{
		AXS_ERROR("axs_download_init fail!\n");
	}
#else
	axs_irq_disable();
	axs_reset_proc(20);
	axs_irq_enable();
#endif
*/
	tpd_zlog_record_notify(TP_ESD_CHECK_ERROR_NO);
	tpd_notifier_call_chain(TP_ESD_CHECK_ERROR);
}
static void esd_check_func(struct work_struct *work)
{
    if (g_axs_data->fw_loading ||(!g_axs_data->esd_host_enable))
    {
        AXS_INFO("skip esd_check_func,fw_loading:%d,esd_host_enable:%d",g_axs_data->fw_loading,g_axs_data->esd_host_enable);
    }
    else
    {
        g_axs_data->tp_no_touch_500ms_count += 1;
        AXS_DEBUG("esd_check_func,500ms_count:%d",g_axs_data->tp_no_touch_500ms_count);
        if (g_axs_data->tp_no_touch_500ms_count>=3)/*no report interrupt for 1.5s */
        {
            //clear count
             AXS_ERROR("ESD check error ,500ms_count:%d",g_axs_data->tp_no_touch_500ms_count);
            g_axs_data->tp_no_touch_500ms_count = 0;
            axs_esd_reset_process();
            axs_release_all_finger();
        }
    }
    if (g_axs_data->esd_host_enable) {
        queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                       msecs_to_jiffies(ESD_CHECK_DELAY_TIME));
    }
}

int axs_esd_check_init(struct axs_ts_data *ts_data)
{
    AXS_FUNC_ENTER();

    if (ts_data->ts_workqueue)
    {
        INIT_DELAYED_WORK(&ts_data->esd_check_work, esd_check_func);
    }
    else
    {
        AXS_ERROR("ts_workqueue is NULL");
        return -EINVAL;
    }

    g_axs_data->esd_host_enable = 1;
    g_axs_data->tp_esd_check_ff_count = 0;

    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                       msecs_to_jiffies(ESD_CHECK_DELAY_TIME*2));

    AXS_FUNC_EXIT();
    return 0;
}

int axs_esd_check_suspend(void)
{
    AXS_FUNC_ENTER();
    cancel_delayed_work(&g_axs_data->esd_check_work);
	g_axs_data->tp_no_touch_500ms_count = 0;
    g_axs_data->tp_esd_check_ff_count = 0;
    g_axs_data->esd_host_enable = 0;
    AXS_FUNC_EXIT();
    return 0;
}

int axs_esd_check_resume( void )
{
    AXS_FUNC_ENTER();
    g_axs_data->tp_no_touch_500ms_count = 0;
    g_axs_data->tp_esd_check_ff_count = 0;
    g_axs_data->esd_host_enable = 1;
    queue_delayed_work(g_axs_data->ts_workqueue, &g_axs_data->esd_check_work,
                           msecs_to_jiffies(ESD_CHECK_DELAY_TIME*2));
    AXS_FUNC_EXIT();
    return 0;
}
#endif

