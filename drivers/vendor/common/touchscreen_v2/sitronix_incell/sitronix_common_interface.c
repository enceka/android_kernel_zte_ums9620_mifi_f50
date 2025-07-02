/************************************************************************
*
* File Name: sitronix_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

#include "sitronix_ts.h"
#include <linux/power_supply.h>
#include "sitronix_st7123.h"

#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_PATH_LEN  64
#define MAX_NAME_LEN_20  20

char sitronix_vendor_name[MAX_NAME_LEN_20] = { 0 };
char sitronix_firmware_name[MAX_FILE_NAME_LEN] = {0};
#ifdef SITRONIX_DEFAULT_FIRMWARE
char sitronix_default_firmware_name[MAX_FILE_NAME_LEN] = {0};
#endif
int sitronix_vendor_id = 0;
int sitronix_tptest_result = 0;
struct ts_firmware *sitronix_adb_upgrade_firmware = NULL;
extern int sitronix_ts_suspend(struct device *dev);
extern int sitronix_ts_resume(struct device *dev);
extern int sitronix_ts_get_fw_revision(struct sitronix_ts_data *ts_data);

extern void sitronix_mt_pause(void);
extern int sitronix_ts_enable_raw(struct sitronix_ts_data *ts_data, int type);
extern int sitronix_ts_get_rawdata(struct sitronix_ts_data *ts_data, int *rbuf);

struct tpvendor_t sitronix_vendor_l[] = {
	{STP_VENDOR_ID_0, STP_VENDOR_0_NAME},
	{STP_VENDOR_ID_1, STP_VENDOR_1_NAME},
	{STP_VENDOR_ID_2, STP_VENDOR_2_NAME},
	{STP_VENDOR_ID_3, STP_VENDOR_3_NAME},
	{VENDOR_END, "Unknown"},
};

int sitronix_get_fw(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(sitronix_vendor_l); i++) {
		if (strnstr(lcd_name, sitronix_vendor_l[i].vendor_name, strlen(lcd_name))) {
			sitronix_vendor_id = sitronix_vendor_l[i].vendor_id;
			strlcpy(sitronix_vendor_name, sitronix_vendor_l[i].vendor_name,
				sizeof(sitronix_vendor_name));
			ret = 0;
			goto out;
		}
	}
	strlcpy(sitronix_vendor_name, "Unknown", sizeof(sitronix_vendor_name));
	ret = -EIO;
out:
	snprintf(sitronix_firmware_name, sizeof(sitronix_firmware_name),
			"sitronix_firmware_%s.dump", sitronix_vendor_name);
#ifdef SITRONIX_DEFAULT_FIRMWARE
	snprintf( sitronix_default_firmware_name, sizeof(sitronix_default_firmware_name),
			"%s_%s.dump", SITRONIX_DEFAULT_FIRMWARE, sitronix_vendor_name);
#endif
	return ret;
}

int sitronix_tp_requeset_firmware(void)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tp_firmware == NULL || !cdev->tp_firmware->size) {
		sterr("cdev->tp_firmware is NULL");
		goto err_free_firmware;
	}

	if (sitronix_adb_upgrade_firmware) {
		kfree(sitronix_adb_upgrade_firmware);
		sitronix_adb_upgrade_firmware = NULL;
	}
	sitronix_adb_upgrade_firmware = kzalloc(sizeof(struct ts_firmware), GFP_KERNEL);
	if (sitronix_adb_upgrade_firmware == NULL) {
		sterr("Request firmware alloc ts_firmware failed");
		return -ENOMEM;
	}

	sitronix_adb_upgrade_firmware->size = cdev->tp_firmware->size;
	sitronix_adb_upgrade_firmware->data = vmalloc(sitronix_adb_upgrade_firmware->size);
	if (sitronix_adb_upgrade_firmware->data == NULL) {
		sterr("Request form file alloc firmware data failed");
		goto err_free_firmware;
	}
	memcpy(sitronix_adb_upgrade_firmware->data, (u8 *)cdev->tp_firmware->data, sitronix_adb_upgrade_firmware->size);
	return 0;
err_free_firmware:
	kfree(sitronix_adb_upgrade_firmware);
	sitronix_adb_upgrade_firmware = NULL;
	return -ENOMEM;
}

static int sitronix_tp_fw_upgrade(struct ztp_device *cdev, char *fw_name, int fwname_len)
{
	int ret = 0;
#ifdef SITRONIX_TP_WITH_FLASH
	gts->flash_powerful_upgrade = 1;
	/* if (buf[0] == '1') {
		stmsg("flash_powerful_upgrade!\n");
		gts->flash_powerful_upgrade = 1;
	} */
#endif /* SITRONIX_TP_WITH_FLASH */
	if (sitronix_tp_requeset_firmware() < 0) {
		sterr("Get firmware from adb upgrade failed");
		goto error_fw_upgrade;
	}
	gts->fw_request_status = 0;
	stmsg("Get firmware from adb upgrade success.\n");
	mutex_lock(&gts->mutex);
	gts->upgrade_result = sitronix_do_upgrade();
	mutex_unlock(&gts->mutex);
	ret = gts->upgrade_result;
#ifdef SITRONIX_TP_WITH_FLASH
	gts->flash_powerful_upgrade = 0;
#endif /* SITRONIX_TP_WITH_FLASH */
	return ret;
error_fw_upgrade:
	return -EIO;
}

static int tpd_init_tpinfo(struct ztp_device *cdev)
{
	struct sitronix_ts_data *ts = gts;

	if (ts->in_suspend) {
		stmsg("%s:In suspended", __func__);
		return -EIO;
	}

	mutex_lock(&ts->mutex);
	sitronix_ts_get_fw_revision(gts);
	mutex_unlock(&ts->mutex);
	strlcpy(cdev->ic_tpinfo.tp_name, "sitronix_ts", sizeof(cdev->ic_tpinfo.tp_name));
	strlcpy(cdev->ic_tpinfo.vendor_name, sitronix_vendor_name, sizeof(cdev->ic_tpinfo.vendor_name));
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_SITRONIX;
	cdev->ic_tpinfo.firmware_ver = ts->ts_dev_info.fw_version;
	cdev->ic_tpinfo.module_id = sitronix_vendor_id;
	return 0;
}

static int sitronix_tp_suspend_show(struct ztp_device *cdev)
{
	return cdev->tp_suspend;
}

static int sitronix_set_tp_suspend(struct ztp_device *cdev, u8 suspend_node, int enable)
{
	if (enable) {
		change_tp_state(LCD_OFF);
	} else {
		change_tp_state(LCD_ON);
	}
	cdev->tp_suspend = enable;
	return cdev->tp_suspend;
}

static int sitronix_tp_resume(void *dev)
{
	struct sitronix_ts_data *ts = gts;

	stmsg("%s enter", __func__);

	return sitronix_ts_resume(&ts->pdev->dev);
}

static int sitronix_tp_suspend(void *dev)
{
	struct sitronix_ts_data *ts = gts;

	stmsg("%s enter", __func__);

	return sitronix_ts_suspend(&ts->pdev->dev);
}

static int tpd_test_cmd_store(struct ztp_device *cdev)
{
	int retry = 0;

	stmsg("%s:enter.\n", __func__);
	sitronix_ts_irq_enable(gts, false);

	gts->upgrade_doing = true;
	mutex_lock(&gts->mutex);
	do {
		sitronix_tptest_result = 0;
		st_self_test();
		if (sitronix_tptest_result) {
			retry++;
			sterr("tp self test failed, retry:%d", retry);
			msleep(20);
		} else {
			break;
		}
	} while (retry < 3);
	if (retry == 3) {
		sterr("selftest failed!");
	} else {
		stmsg("selftest success!");
	}
	stmsg("self_test_result is %d.\n", gts->self_test_result);
	mutex_unlock(&gts->mutex);

	sitronix_ts_mt_reset_process();
	gts->upgrade_doing = false;

	//Enable IRQ
	sitronix_ts_irq_enable(gts, true);
	return 0;
}

static int tpd_test_cmd_show(struct ztp_device *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;

	stmsg("%s:enter.\n", __func__);

	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", sitronix_tptest_result, gts->ts_dev_info.x_chs, gts->ts_dev_info.y_chs, 0);
	stmsg("tpd  test:%s.\n", buf);

	num_read_chars = i_len;
	return num_read_chars;
}

static int sitronix_print_data2buffer(struct ztp_device *cdev, int *frame_data_words, enum tp_test_type test_type, int *len)
{
	unsigned int row = 0;
	unsigned int col = 0;
	int i, j;

	row = gts->ts_dev_info.x_chs;
	col = gts->ts_dev_info.y_chs;

	switch (test_type) {
	case RAWDATA_TEST:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"RawData:\n");
		break;
	case DELTA_TEST:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"DiffData:\n");
		break;
	default:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"Unknow:\n");
	}

	for (j = 0; j < col; ++j) {
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
			"HXTP[%2d]", j + 1);
		for (i = 0; i < row; ++i) {
			*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"%5d,", frame_data_words[i * col + j]);
		}
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len, "\n");
	}
	*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len, "\n\n");
	return *len;
}

static int sitronix_data_request(int *frame_data_words, enum tp_test_type test_type)
{
	int ret = 0;
	int command = 0;
	int total_size = 0;
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int col = 0;
	unsigned int row = 0;

	row = gts->ts_dev_info.x_chs;
	col = gts->ts_dev_info.y_chs;
	total_size = (row * col);
	if (frame_data_words == NULL) {
		ret = -ENOMEM;
		goto SUB_END;
	}

	memset(frame_data_words, 0, total_size);
	switch (test_type) {
	case RAWDATA_TEST:
		command = 0x01;
		break;
	case DELTA_TEST:
		command = 0x02;
		break;
	default:
		sterr("%s:the Para is error!\n", __func__);
		ret = -1;
		goto SUB_END;
	}

	sitronix_ts_enable_raw(gts, command);

	ret = sitronix_ts_get_rawdata(gts, frame_data_words);
	if(ret < 0)
		sterr("failed to read rawdata (%d)\n", ret);
	else
		ret = 0;

	for (y = 0; y < col; ++y) {
		pr_cont("STP[%2d]", y + 1);
		for (x = 0; x < row; ++x) {
			pr_cont("%5d,", frame_data_words[col * x + y]);
		}
		pr_cont("\n");
	}

SUB_END:
	return ret;
}

static int sitronix_testing_delta_raw_report(struct ztp_device *cdev, u8 num_of_reports)
{
	int *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0;
	int retval = 0;
	int len = 0;

	row = gts->ts_dev_info.x_chs;
	col = gts->ts_dev_info.y_chs;

	mutex_lock(&gts->mutex);
	sitronix_mt_pause();
	frame_data_words = (int *)kcalloc((row * col), sizeof(int), GFP_KERNEL);
	if (frame_data_words ==  NULL) {
		sterr("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"frame: %d, TX:%d  RX:%d\n", idx, col, row);
		retval = sitronix_data_request(frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			sterr("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = sitronix_print_data2buffer(cdev, frame_data_words, RAWDATA_TEST, &len);
		if (retval <= 0) {
			sterr("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		retval = sitronix_data_request(frame_data_words, DELTA_TEST);
		if (retval < 0) {
			sterr("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = sitronix_print_data2buffer(cdev, frame_data_words, DELTA_TEST, &len);
		if (retval <= 0) {
			sterr("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}

	}

	retval = 0;
	msleep(20);
	stmsg("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	sitronix_ts_enable_raw(gts, 0);
	sitronix_mt_restore();
	mutex_unlock(&gts->mutex);
	return retval;
}

static int sitronix_tpd_get_noise(struct ztp_device *cdev)
{
	int retval;

	if (gts->in_suspend)
		return -EIO;

	if(tp_alloc_tp_firmware_data(10 * RT_DATA_LEN)) {
		sterr(" alloc tp firmware data fail");
		return -ENOMEM;
	}

	retval = sitronix_testing_delta_raw_report(cdev, 5);
	if (retval < 0) {
		sterr("%s: get_raw_noise failed!\n",  __func__);
		return retval;
	}
	return 0;
}

static int sitronix_headset_state_show(struct ztp_device *cdev)
{
	stmsg("%s: headset_state = %d.\n", __func__, cdev->headset_state);
	return cdev->headset_state;
}

static int sitronix_set_headset_state(struct ztp_device *cdev, int enable)
{
	cdev->headset_state = enable;
	stmsg("%s: headset_state = %d\n", __func__, cdev->headset_state);
	mutex_lock(&gts->mutex);
	if (!gts->in_suspend) {
		if (cdev->headset_state)
			sitronix_mode_switch(ST_MODE_HEADPHONE, true);
		else
			sitronix_mode_switch(ST_MODE_HEADPHONE, false);
	}
	mutex_unlock(&gts->mutex);
	return cdev->headset_state;
}

static int sitronix_set_display_rotation(struct ztp_device *cdev, int mrotation)
{
	cdev->display_rotation = mrotation;
	if (gts->in_suspend)
		return 0;
	stmsg("%s: display_rotation = %d.\n", __func__, cdev->display_rotation);
	mutex_lock(&gts->mutex);
	/* sitronix require swap 90 and 270 */
	switch (cdev->display_rotation) {
		case mRotatin_0:
			sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_0);
			break;
		case mRotatin_90:
			sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_90);
			break;
		case mRotatin_180:
			sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_180);
			break;
		case mRotatin_270:
			sitronix_mode_switch_value(ST_MODE_GRIP, true, ST_MODE_GRIP_ROTATE_270);
			break;
		default:
			break;
	}
	mutex_unlock(&gts->mutex);
	return cdev->display_rotation;
}

int sitronix_register_fw_class(void)
{
	struct sitronix_ts_data *ts = gts;

	sitronix_get_fw();
	tpd_cdev->get_tpinfo = tpd_init_tpinfo;
	tpd_cdev->tp_fw_upgrade = sitronix_tp_fw_upgrade;
	tpd_cdev->tp_suspend_show = sitronix_tp_suspend_show;
	tpd_cdev->set_tp_suspend = sitronix_set_tp_suspend;

	tpd_cdev->tp_data = ts;
	tpd_cdev->tp_resume_func = sitronix_tp_resume;
	tpd_cdev->tp_suspend_func = sitronix_tp_suspend;

	tpd_cdev->tp_self_test = tpd_test_cmd_store;
	tpd_cdev->get_tp_self_test_result = tpd_test_cmd_show;
	tpd_cdev->set_display_rotation = sitronix_set_display_rotation;
	tpd_cdev->headset_state_show = sitronix_headset_state_show;
	tpd_cdev->set_headset_state = sitronix_set_headset_state;

	tpd_cdev->max_x = ts->ts_dev_info.x_res;
	tpd_cdev->max_y = ts->ts_dev_info.y_res;
	tpd_cdev->input = ts->input_dev;

	tpd_cdev->get_noise = sitronix_tpd_get_noise;

	/* gts->charger_workqueue = create_singlethread_workqueue("sitronix_ts_charger_workqueue");
	if (!gts->charger_workqueue) {
		sterr(" allocate gts->charger_workqueue failed\n");
	} else  {
		gts->charger_mode = false;
		INIT_DELAYED_WORK(&gts->charger_work, sitronix_work_charger_detect_work);
		queue_delayed_work(gts->charger_workqueue, &gts->charger_work, msecs_to_jiffies(1000));
		sitronix_init_charger_notifier();
	} */
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	zlog_tp_dev.device_name = sitronix_vendor_name;
	zlog_tp_dev.ic_name = "sitronix_tp";
	TPD_ZLOG("device_name:%s, ic_name: %s.", zlog_tp_dev.device_name, zlog_tp_dev.ic_name);
#endif
	return 0;
}


