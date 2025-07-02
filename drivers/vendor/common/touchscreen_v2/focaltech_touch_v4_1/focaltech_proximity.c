/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
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

/*****************************************************************************
*
* File Name: focaltech_proximity.c
*
*    Author: Focaltech Driver Team
*
*   Created: 2016-09-19
*
*  Abstract: close proximity function
*
*   Version: v1.0
*
* Revision History:
*        v1.0:
*            First release based on xiaguobin's solution. By luougojin 2016-08-19
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"
#include "focaltech_common.h"

#if FTS_PSENSOR_EN
/* #include <hwmsensor.h>
#include <sensors_io.h>
#include <alsps.h> */

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
/*
 * FTS_ALSPS_SUPPORT is choose structure hwmsen_object or control_path, data_path
 * FTS_ALSPS_SUPPORT = 1, is control_path, data_path
 * FTS_ALSPS_SUPPORT = 0, hwmsen_object
 */
/* #define FTS_ALSPS_SUPPORT            1 */
/*
 * FTS_OPEN_DATA_HAL_SUPPORT is choose structure ps_control_path or batch, flush
 * FTS_ALSPS_SUPPORT = 1, is batch, flush
 * FTS_ALSPS_SUPPORT = 0, NULL
 */
#define FTS_OPEN_DATA_HAL_SUPPORT    1
#define PS_FAR_AWAY                  1
#define PS_NEAR                      0

/* #if !FTS_ALSPS_SUPPORT
#include <hwmsen_dev.h>
#endif */

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#define TP_PS_INPUT_DEV "proximity_tp"

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct fts_proximity_st {
    u8      mode                : 1;    /* 1- proximity enable 0- disable */
    u8      detect              : 1;    /* 0-->close ; 1--> far away */
    u8      unused              : 4;
};

/*****************************************************************************
* Static variables
*****************************************************************************/
/* static struct fts_proximity_st fts_proximity_data; */
static struct wakeup_source *tp_ps_suspend_lock;
static u8 tp_ps_suspend_lock_flag = 0;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
int get_ps_mode_data(unsigned char *mode_data,  struct fts_ts_data *ts_data);
int tpd_get_ps_value(struct fts_ts_data *ts_data);
int tpd_enable_ps(struct fts_ts_data *ts_data, int enable);
int fts_proximity_init(struct fts_ts_data *ts_data);

#if defined (HUB_TP_PS_ENABLE) && ( HUB_TP_PS_ENABLE== 1)
static struct class ps_sensor_class = {
	.name = "tp_ps",
	.owner = THIS_MODULE,
};

static ssize_t delay_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", 200);
}

static ssize_t delay_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	return count;
}

static CLASS_ATTR_RW(delay);


static ssize_t enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 64, "%d\n", fts_data->tpd_proximity_flag);
}

static ssize_t enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int enable;
	int ret = 0;
	int handle;
	FTS_FUNC_ENTER();

	ret = sscanf(buf, "%d %d\n", &handle, &enable);

	if (ret != 2) {
		FTS_ERROR("%s: sscanf tp_ps enable data error!!! ret = %d \n", __func__, ret);
		return -EINVAL;
	}

	if (!fts_data->ft6x06_proximity_input_dev) {
		FTS_INFO("enable psensor fail : have no input dev\n");
		return count;
	}

    /* if(!tpd_get_ps_value(fts_data) && enable) {
        FTS_INFO("tp proximity is already enable.");
        return count;
    } */

	mutex_lock(&fts_data->ft6x06_proximity_input_dev->mutex);
	enable = (enable > 0) ? 1 : 0;
	fts_data->tpd_proximity_detect_is_far = 1;
	fts_data->tpd_proximity_flag = enable;
	if (!fts_data->suspended) {
		tpd_enable_ps(fts_data, enable);
	}

	if (enable) {
		change_psensor_state(ENABLE_PSENSOR);
		/* init value far for vts test*/
		input_report_abs(fts_data->ft6x06_proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(fts_data->ft6x06_proximity_input_dev);
	} else {
		change_psensor_state(DISABLE_PSENSOR);
	}
	msleep(100);
	mutex_unlock(&fts_data->ft6x06_proximity_input_dev->mutex);
	return count;
}

static CLASS_ATTR_RW(enable);

static ssize_t flush_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 64, "%d\n", fts_data->tpd_proximity_flag);
}

static ssize_t flush_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	static int flush_count = 0;

	if (flush_count % 2 == 0) {
		input_report_abs(fts_data->ft6x06_proximity_input_dev, ABS_DISTANCE, -1);
		flush_count = 1;
	} else {
		input_report_abs(fts_data->ft6x06_proximity_input_dev, ABS_DISTANCE, -2);
		flush_count = 0;
	}
	input_sync(fts_data->ft6x06_proximity_input_dev);

	return count;
}

static CLASS_ATTR_RW(flush);

static ssize_t batch_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	return count;
}

static CLASS_ATTR_WO(batch);
#endif

#if 0
/************************************************************************
* Name: fts_enter_proximity_mode
* Brief:  change proximity mode
* Input:  proximity mode
* Output: no
* Return: success =0
***********************************************************************/
static int fts_enter_proximity_mode(int mode)
{
    int ret = 0;
    u8 buf_addr = 0;
    u8 buf_value = 0;

    buf_addr = FTS_REG_FACE_DEC_MODE_EN;
    if (mode)
        buf_value = 0x01;
    else
        buf_value = 0x00;

    ret = fts_write_reg(buf_addr, buf_value);
    if (ret < 0) {
        FTS_ERROR("[PROXIMITY] Write proximity register(0xB0) fail!");
        return ret;
    }

    fts_proximity_data.mode = buf_value ? ENABLE : DISABLE;
    FTS_DEBUG("[PROXIMITY] proximity mode = %d", fts_proximity_data.mode);
    return 0 ;
}

/*****************************************************************************
*  Name: fts_proximity_recovery
*  Brief: need call when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_proximity_recovery(struct fts_ts_data *ts_data)
{
    int ret = 0;

    if (fts_proximity_data.mode)
        ret = fts_enter_proximity_mode(ENABLE);

    return ret;
}

#if FTS_ALSPS_SUPPORT
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int ps_open_report_data(int open)
{
    /* should queue work to report event if  is_report_input_direct=true */
    return 0;
}

/* if use  this type of enable , Psensor only enabled but not report inputEvent to HAL */
static int ps_enable_nodata(int en)
{
    int err = 0;

    FTS_DEBUG("[PROXIMITY]SENSOR_ENABLE value = %d", en);
    /* Enable proximity */
    mutex_lock(&fts_data->input_dev->mutex);
    err = fts_enter_proximity_mode(en);
    mutex_unlock(&fts_data->input_dev->mutex);
    return err;
}

static int ps_set_delay(u64 ns)
{
    return 0;
}
#if FTS_OPEN_DATA_HAL_SUPPORT
static int ps_batch(int flag, int64_t sampling_period_ns, int64_t max_batch_report_ns)
{
    return 0;
}

static int ps_flush(void)
{
    return 0;
}
#endif
static int ps_get_data(int *value, int *status)
{
    *value = (int)fts_proximity_data.detect;
    FTS_DEBUG("fts_proximity_data.detect = %d\n", *value);
    *status = SENSOR_STATUS_ACCURACY_MEDIUM;
    return 0;
}
int ps_local_init(void)
{
    int err = 0;
    struct ps_control_path ps_ctl = { 0 };
    struct ps_data_path ps_data = { 0 };

    ps_ctl.is_use_common_factory = false;
    ps_ctl.open_report_data = ps_open_report_data;
    ps_ctl.enable_nodata = ps_enable_nodata;
    ps_ctl.set_delay = ps_set_delay;
#if FTS_OPEN_DATA_HAL_SUPPORT
    ps_ctl.batch = ps_batch;
    ps_ctl.flush = ps_flush;
#endif
    ps_ctl.is_report_input_direct = false;
    ps_ctl.is_support_batch = false;

    err = ps_register_control_path(&ps_ctl);
    if (err) {
        FTS_ERROR("register fail = %d\n", err);
    }
    ps_data.get_data = ps_get_data;
    ps_data.vender_div = 100;
    err = ps_register_data_path(&ps_data);
    if (err) {
        FTS_ERROR("tregister fail = %d\n", err);
    }

    return err;
}
int ps_local_uninit(void)
{
    return 0;
}

struct alsps_init_info ps_init_info = {
    .name = "fts_ts",
    .init = ps_local_init,
    .uninit = ps_local_uninit,
};

#else

/*****************************************************************************
*  Name: fts_ps_operate
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ps_operate(void *self, uint32_t command, void *buff_in, int size_in, void *buff_out, int size_out,
                          int *actualout)
{
    int err = 0;
    int value;
    struct hwm_sensor_data *sensor_data;

    FTS_DEBUG("[PROXIMITY]COMMAND = %d", command);
    switch (command) {
    case SENSOR_DELAY:
        if ((buff_in == NULL) || (size_in < sizeof(int))) {
            FTS_ERROR("[PROXIMITY]Set delay parameter error!");
            err = -EINVAL;
        }
        break;

    case SENSOR_ENABLE:
        if ((buff_in == NULL) || (size_in < sizeof(int))) {
            FTS_ERROR("[PROXIMITY]Enable sensor parameter error!");
            err = -EINVAL;
        } else {
            value = *(int *)buff_in;
            FTS_DEBUG("[PROXIMITY]SENSOR_ENABLE value = %d", value);
            /* Enable proximity */
            err = fts_enter_proximity_mode(value);
        }
        break;

    case SENSOR_GET_DATA:
        if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
            FTS_ERROR("[PROXIMITY]get sensor data parameter error!");
            err = -EINVAL;
        } else {
            sensor_data = (struct hwm_sensor_data *)buff_out;
            sensor_data->values[0] = (int)fts_proximity_data.detect;
            FTS_DEBUG("sensor_data->values[0] = %d", sensor_data->values[0]);
            sensor_data->value_divide = 1;
            sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
        }
        break;
    default:
        FTS_ERROR("[PROXIMITY]ps has no operate function:%d!", command);
        err = -EPERM;
        break;
    }

    return err;
}
#endif

/*****************************************************************************
*  Name: fts_proximity_readdata
*  Brief:
*  Input:
*  Output:
*  Return: 0 - need return in suspend
*****************************************************************************/
int fts_proximity_readdata(struct fts_ts_data *ts_data)
{
    int ret;
    int proximity_status = 1;
    u8  regvalue;
#if !FTS_ALSPS_SUPPORT
    struct hwm_sensor_data sensor_data;
#endif
    if (fts_proximity_data.mode == DISABLE)
        return -EPERM;

    fts_read_reg(FTS_REG_FACE_DEC_MODE_STATUS, &regvalue);

    if (regvalue == 0xC0) {
        /* close. need lcd off */
        proximity_status = PS_NEAR;
    } else if (regvalue == 0xE0) {
        /* far away */
        proximity_status = PS_FAR_AWAY;
    }

    FTS_INFO("fts_proximity_data.detect is %d", fts_proximity_data.detect);

    if (proximity_status != (int)fts_proximity_data.detect) {
        FTS_DEBUG("[PROXIMITY] p-sensor state:%s", proximity_status ? "AWAY" : "NEAR");
        fts_proximity_data.detect = proximity_status ? PS_FAR_AWAY : PS_NEAR;
#if FTS_ALSPS_SUPPORT
        ret = ps_report_interrupt_data(fts_proximity_data.detect);
#else
        sensor_data.values[0] = proximity_status;
        sensor_data.value_divide = 1;
        sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
        ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);
        if (ret) {
            FTS_ERROR("[PROXIMITY] Call hwmsen_get_interrupt_data failed, ret=%d", ret);
            return ret;
        }
#endif
        return 0;
    }

    return -1;
}

/*****************************************************************************
*  Name: fts_proximity_suspend
*  Brief: Run when tp enter into suspend
*  Input:
*  Output:
*  Return: 0 - need return in suspend
*****************************************************************************/
int fts_proximity_suspend(void)
{
    if (fts_proximity_data.mode == ENABLE)
        return 0;
    else
        return -1;
}

/*****************************************************************************
*  Name: fts_proximity_resume
*  Brief: Run when tp resume
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_proximity_resume(void)
{
    if (fts_proximity_data.mode == ENABLE)
        return 0;
    else
        return -1;
}
#endif

void fts_proximity_recovery(struct fts_ts_data *ts_data)
{
	if(ts_data->tpd_proximity_flag) {
		tpd_enable_ps(ts_data, ENABLE);
	}
}

int get_ps_mode_data(unsigned char *mode_data, struct fts_ts_data *ts_data)
{
	unsigned char ps_mode;
	int ret;
	//ret = fts_read_reg(0x01, &ps_mode);
	ret = fts_read_reg(0xB5, &ps_mode); /*near 1,far away 0*/
	 /*0xE0 -- far away */
	 /*0xC0 -- near. need lcd off */
	/* FTS_INFO("ps_mode reg = 0x%x\n",  ps_mode); */

	*mode_data = ps_mode;
	return ret;
}

int tpd_get_ps_value(struct fts_ts_data *ts_data)
{
	return ts_data->tpd_proximity_detect_is_far;
}

int tpd_enable_ps(struct fts_ts_data *ts_data, int enable)
{
	u8 data;
	u8 read_val = 0;
	int i = 0, ret = 0;
	int back;
	FTS_INFO("tpd_enable_ps: %s\n", enable? "enable" : "disable");
	if (ts_data->fts_is_earlysuspend_flag) {
		FTS_INFO("****tpd_enable_ps fail for Tp earlysuspend*****\n");
		ts_data->tpd_proximity_flag = 0;
		ts_data->psensorcall = enable;
		ret = 0;
		return ret;
	}
	if (enable) {
		if (tp_ps_suspend_lock_flag == 0) {
			__pm_stay_awake(tp_ps_suspend_lock);
			tp_ps_suspend_lock_flag = 1;
	}
	} else {
		if (tp_ps_suspend_lock_flag == 1) {
			__pm_relax(tp_ps_suspend_lock);
			tp_ps_suspend_lock_flag = 0;
		}
	}

	if (enable) {
		data = 0x1;
		do {
			back = fts_write_reg(0xB0, data);
			mdelay(2);
			FTS_INFO("back = %d \n", back);
			fts_read_reg(0xB0, &read_val);
			i++;
			if ((i % 3) == 0 ) {
				FTS_ERROR("Error: line %d, tpd_enable_ps error,need to reset ftxx!\n", __LINE__);
				return 0;
			}
		} while ((read_val != 1) && (i < 15));
		if (read_val != 1) {
			FTS_INFO("Error: line %d, tpd_enable_ps error!\n", __LINE__);
			ts_data->tpd_proximity_flag = 0;
			ret = 1;
		} else {
			FTS_INFO("Success: line %d, tpd_enable_ps success!\n", __LINE__);
			ts_data->tpd_proximity_flag = 1;
		}
	} else {
		data = 0x0;
		fts_write_reg(0xB0, data);
		ts_data->tpd_proximity_flag = 0;
		FTS_INFO("Success: line %d, tpd_disable_ps success!\n", __LINE__);
	}
	ts_data->psensorcall = 0;
	return ret;
}

int fts_proximity_init(struct fts_ts_data *ts_data)
{
	int err = 0;

	FTS_FUNC_ENTER();
#if 0
#if !FTS_ALSPS_SUPPORT
    struct hwmsen_object obj_ps;
#endif

    memset((u8 *)&fts_proximity_data, 0, sizeof(struct fts_proximity_st));
    fts_proximity_data.detect = PS_FAR_AWAY;  /* defalut far awway */

#if FTS_ALSPS_SUPPORT
    alsps_driver_add(&ps_init_info);
#else
    obj_ps.polling = 0; /* interrupt mode */
    obj_ps.sensor_operate = fts_ps_operate;
    err = hwmsen_attach(ID_PROXIMITY, &obj_ps);
    if (err)
        FTS_ERROR("[PROXIMITY]fts proximity attach fail = %d!", err);
    else
        FTS_INFO("[PROXIMITY]fts proximity attach ok = %d\n", err);
#endif
#endif
	ts_data->tpd_proximity_detect_is_far = 1; //0-->near ; 1--> far away 
	tp_ps_suspend_lock = wakeup_source_register(NULL, "ps wakelock");
	/* allocate proximity input_device */
	ts_data->ft6x06_proximity_input_dev = input_allocate_device();
	if (!ts_data->ft6x06_proximity_input_dev)
	{
		FTS_INFO("could not allocate input device\n");
		return -1;
	}
	FTS_INFO("[FTS_TS]: GZL TP_PS init1\n");
	input_set_drvdata(ts_data->ft6x06_proximity_input_dev, ts_data);
	ts_data->ft6x06_proximity_input_dev->name = TP_PS_INPUT_DEV;
	ts_data->ft6x06_proximity_input_dev->phys = TP_PS_INPUT_DEV;
	input_set_capability(ts_data->ft6x06_proximity_input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(ts_data->ft6x06_proximity_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

#if defined (HUB_TP_PS_ENABLE) && (HUB_TP_PS_ENABLE == 1)
	/*add class sysfs for tp_ps*/
	err = class_register(&ps_sensor_class);
	if (err < 0) {
		FTS_ERROR("Create fsys class failed (%d)\n", err);
		goto err_class_creat;
	}

	err = class_create_file(&ps_sensor_class, &class_attr_delay);
	if (err < 0) {
		FTS_ERROR("Create delay file failed (%d)\n", err);
		goto exit_unregister_class;
	}

	err = class_create_file(&ps_sensor_class, &class_attr_enable);
	if (err < 0) {
		FTS_ERROR("Create enable file failed (%d)\n", err);
		goto exit_unregister_class;
	}

	err = class_create_file(&ps_sensor_class, &class_attr_flush);
	if (err < 0) {
		FTS_ERROR("Create flush file failed (%d)\n", err);
		goto exit_unregister_class;
	}

	err = class_create_file(&ps_sensor_class, &class_attr_batch);
	if (err < 0) {
		FTS_ERROR("Create batch file failed (%d)\n", err);
		goto exit_unregister_class;
	}
#endif

	err = input_register_device(ts_data->ft6x06_proximity_input_dev);
	if (err < 0)
	{
		FTS_INFO("could not register psensor input device\n");
		goto free_psensor_input_dev;
	}

	FTS_FUNC_EXIT();
	return 0;

free_psensor_input_dev:
#if defined (HUB_TP_PS_ENABLE) && (HUB_TP_PS_ENABLE == 1)
exit_unregister_class:
	FTS_INFO("unregister tp_ps_sensor_class.\n");
	class_unregister(&ps_sensor_class);
err_class_creat:
#endif
	input_free_device(ts_data->ft6x06_proximity_input_dev);
	return -1;
}

/* int fts_proximity_exit(void)
{
    return 0;
} */
#endif /* FTS_PSENSOR_EN */

