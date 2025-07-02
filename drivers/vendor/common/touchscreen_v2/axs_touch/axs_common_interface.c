/************************************************************************
*
* File Name: axs_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/

#include "axs_core.h"
#include "axs_test.h"

#define MAX_FILE_NAME_LEN       64
#define MAX_FILE_PATH_LEN  64
#define MAX_NAME_LEN_20  20
#define AXS_MAX_FW_LEN   (128 * 1024)

extern int axs_ts_resume(struct device *dev);
extern int axs_ts_suspend(struct device *dev);
extern bool axs_download_app(struct axs_upgrade *upg);
extern struct host_test *axs_test;
extern struct mutex s_device_mutex;
extern struct upgrade_func download_func_15205;
extern struct axs_upgrade *fwupgrade;

int axs_tp_test_result = 0;
char axs_vendor_name[MAX_NAME_LEN_20] = { 0 };
char axs_firmware_name[MAX_FILE_NAME_LEN] = {0};
char axs_ini_filename[MAX_FILE_NAME_LEN] = {0};
int axs_vendor_id = 0;
struct axs_upgrade *adb_fwupgrade = NULL;
#ifdef AXS_DEFAULT_FIRMWARE
char axs_default_firmware_name[50] = {0};
#endif

struct tpvendor_t axs_vendor_l[] = {
	{AXS_VENDOR_ID_0, AXS_VENDOR_0_NAME},
	{AXS_VENDOR_ID_1, AXS_VENDOR_1_NAME},
	{AXS_VENDOR_ID_2, AXS_VENDOR_2_NAME},
	{AXS_VENDOR_ID_3, AXS_VENDOR_3_NAME},
	{VENDOR_END, "Unknown"},
};

int axs_get_fw(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(axs_vendor_l); i++) {
		if (strnstr(lcd_name, axs_vendor_l[i].vendor_name, strlen(lcd_name))) {
			axs_vendor_id = axs_vendor_l[i].vendor_id;
			strlcpy(axs_vendor_name, axs_vendor_l[i].vendor_name,
				sizeof(axs_vendor_name));
			ret = 0;
			goto out;
		}
	}
	strlcpy(axs_vendor_name, "Unknown", sizeof(axs_vendor_name));
	ret = -EIO;
out:
	snprintf(axs_firmware_name, sizeof(axs_firmware_name),
			"axs_firmware_%s.bin", axs_vendor_name);
	snprintf(axs_ini_filename, sizeof(axs_ini_filename),
			"axs_self_test_%s.ini", axs_vendor_name);
#ifdef AXS_DEFAULT_FIRMWARE
	snprintf(axs_default_firmware_name, sizeof(axs_default_firmware_name),
		"%s_%s.bin", AXS_DEFAULT_FIRMWARE, axs_vendor_name);
#endif
	return ret;
}

static int tpd_init_tpinfo(struct ztp_device *cdev)
{
	u16 g_axs_fw_ver = 0;
	struct axs_ts_data *ts_data = g_axs_data;

	if (ts_data->suspended || !tpd_cdev->fw_ready) {
		AXS_INFO("%s:In suspended", __func__);
		return -EIO;
	}
	if (!axs_fwupg_get_ver_in_tp(&g_axs_fw_ver)) {
		AXS_INFO("firmware read version fail");
	}
	AXS_INFO("firmware version=0x%4x\n",g_axs_fw_ver);
	snprintf(cdev->ic_tpinfo.vendor_name, sizeof(cdev->ic_tpinfo.vendor_name), axs_vendor_name);
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "axs_touch");

	cdev->ic_tpinfo.chip_model_id = TS_CHIP_ASX;
	cdev->ic_tpinfo.firmware_ver = g_axs_fw_ver;
	return 0;
}

int  axs_tp_suspend(void *axs_data)
{
	struct axs_ts_data *ts = (struct axs_ts_data *)axs_data;

	axs_ts_suspend(ts->dev);
#ifdef CONFIG_TOUCHSCREEN_POINT_REPORT_CHECK
	cancel_delayed_work_sync(&tpd_cdev->point_report_check_work);
#endif
	return 0;
}

int  axs_tp_resume(void *axs_data)
{
	struct axs_ts_data *ts = (struct axs_ts_data *)axs_data;

	axs_ts_resume(ts->dev);
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	tpd_cdev->tp_reset_timer = jiffies;
#endif
	return 0;
}

static int axs_get_tp_suspend(struct ztp_device *cdev)
{
	struct axs_ts_data *ts_data = g_axs_data;

	AXS_DEBUG("%s enter", __func__);
	cdev->tp_suspend = ts_data->suspended;

	AXS_INFO("%s:tp_suspend=%d", __func__, cdev->tp_suspend);
	AXS_DEBUG("%s exit", __func__);
	return cdev->tp_suspend;
}

static int axs_set_tp_suspend(struct ztp_device *cdev, u8 suspend_node, int enable)
{
	int retval = 0;
	AXS_DEBUG("%s enter", __func__);

	if (enable) {
		change_tp_state(LCD_OFF);
	} else {
		change_tp_state(LCD_ON);
	}

	AXS_INFO("%s:tp_suspend=%d", __func__, enable);
	AXS_DEBUG("%s exit", __func__);
	return retval;
}

#if AXS_HEADSET_EN
void axs_headset_enable(bool enable)
{
	u8 cmd_type[2] = {AXS_REG_HEADSET_WRITE, 1};

	mutex_lock(&s_device_mutex);
	cmd_type[1] = enable;
	axs_write_buf(cmd_type, 2);

	AXS_INFO("set headset enable:%d", enable);
    mutex_unlock(&s_device_mutex);
}

static int axs_get_headset_state(struct ztp_device *cdev)
{
	AXS_INFO("%s enter", __func__);

	cdev->headset_state = g_axs_data->headset_enable;

	AXS_INFO("%s:headset_state=%d", __func__, cdev->headset_state);
	return  g_axs_data->headset_enable;
}

static int axs_set_headset_state(struct ztp_device *cdev, int enable)
{
	struct axs_ts_data *ts_data = g_axs_data;

	AXS_INFO("%s enter", __func__);

	g_axs_data->headset_enable = enable;
	AXS_INFO("%s:headset_state=%d", __func__, g_axs_data->headset_enable);
	if (!ts_data->suspended && tpd_cdev->fw_ready) {
		axs_headset_enable(g_axs_data->headset_enable);
	}
	return g_axs_data->headset_enable;
}
#endif

#if AXS_CHARGE_EN
void axs_charger_enable(bool enable)
{
	u8 cmd_type[2] = {AXS_REG_CHARGE_WRITE, 1};

    mutex_lock(&s_device_mutex);
	cmd_type[1] = enable;

    axs_write_buf(cmd_type, 2);
    AXS_INFO("set charge enable:%d",enable);
    mutex_unlock(&s_device_mutex);
}

static int axs_charger_state_notify(struct ztp_device *cdev)
{
	struct axs_ts_data *ts_data = g_axs_data;
	bool charger_mode_old = g_axs_data->charge_enable;

	g_axs_data->charge_enable = cdev->charger_mode;
	AXS_INFO("set charge enable:%d",g_axs_data->charge_enable);
	if(!ts_data->suspended && tpd_cdev->fw_ready && (g_axs_data->charge_enable != charger_mode_old)) {
		axs_charger_enable(g_axs_data->charge_enable);
	}
	return 0;
}
#endif

#ifdef AXS_ROTATION_EN
static int axs_set_rotation(int angle)
{
    u8 cmd_type[2] = {AXS_REG_ROTATION_WRITE, 0};

    mutex_lock(&s_device_mutex);

	switch(angle) {
	case mRotatin_0:
	case mRotatin_180:
		g_axs_data->rotation_mode = 0;
		break;
	case mRotatin_90:
		g_axs_data->rotation_mode = 1;
		break;
	case mRotatin_270:
		g_axs_data->rotation_mode = 2;
		break;
	default:
		g_axs_data->rotation_mode = 0;
		break;
	}
	cmd_type[1] = g_axs_data->rotation_mode;

	axs_write_bytes(cmd_type, 2);

    mutex_unlock(&s_device_mutex);

	return 0;
}

static int axs_set_display_rotation(struct ztp_device *cdev, int mrotation)
{
	struct axs_ts_data *ts_data = g_axs_data;

	AXS_INFO("%s enter", __func__);

	cdev->display_rotation = mrotation;
	if (ts_data->suspended || !tpd_cdev->fw_ready) {
		AXS_ERROR("%s:In suspended", __func__);
		return -EIO;
	}

	AXS_INFO("%s:display_rotation=%d", __func__, cdev->display_rotation);
	axs_set_rotation(cdev->display_rotation);

	return cdev->display_rotation;
}
#endif

static int axs_fw_upgrade(struct ztp_device *cdev, char *fw_name, int fwname_len)
{
	//struct axs_upgrade *upg;

	AXS_INFO("function begin");
	if (cdev->tp_firmware == NULL) {
		AXS_ERROR("cdev->tp_firmware is NULL");
		return -EIO;
	}
	if (adb_fwupgrade == NULL) {
 		adb_fwupgrade = kzalloc(sizeof(struct axs_upgrade), GFP_KERNEL);
 		if (adb_fwupgrade == NULL) {
			AXS_ERROR("malloc memory for upgrade fail");
 			return -EIO;
		}
		adb_fwupgrade->func =  &download_func_15205;
	}
	if (adb_fwupgrade->fw  == NULL) {
		adb_fwupgrade->fw = vmalloc(cdev->tp_firmware->size);
		if (adb_fwupgrade->fw  == NULL) {
			AXS_ERROR("Request form file alloc firmware data failed");
			goto err_free_adb_fwupgrade;
		}
	}
	adb_fwupgrade->fw_length = cdev->tp_firmware->size;
	memcpy(adb_fwupgrade->fw, (u8 *)cdev->tp_firmware->data, cdev->tp_firmware->size);
	fwupgrade = adb_fwupgrade;
	g_axs_data->fw_loading = 1;
 	axs_irq_disable();
	AXS_INFO("fw bin file len:%x", fwupgrade->fw_length);

 	if ((fwupgrade->fw_length  > AXS_MAX_FW_LEN) || (fwupgrade->fw_length < 1024)) {
		AXS_ERROR("get fw bin file fail");
		goto err;
	} else {
		//axs_fwupg_upgrade(fwupgrade, true);
		tpd_cdev->fw_ready = false;
		axs_download_app(fwupgrade);
		tpd_cdev->fw_ready = true;
	}

err:
	axs_irq_enable();
	g_axs_data->fw_loading = 0;
	AXS_INFO("function end");
	return 0;
err_free_adb_fwupgrade:
	kfree(adb_fwupgrade);
	return -EIO;
}

static int axs_data_request(struct ztp_device *cdev, s16 *frame_data_words, enum tp_test_type  test_type)
{
	int  ret = 0;
	int x = 0, y = 0, i = 0;
	u8 cols = 0;
	u8 rows = 0;
	int index = 0;
	u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_RAWDATA_READ}; // read tx,rx

	rows = RX_NUM;
	cols = TX_NUM;
	switch (test_type) {

	case RAWDATA_TEST:
        write_reg[4]=AXS_FREG_RAWDATA_READ; // read rawdata
		AXS_INFO("set raw data cmd");
		break;
	case DELTA_TEST:
		write_reg[4]=AXS_FREG_DIFF_READ; // read diff
		AXS_INFO("set diff data cmd");
		break;
	default:
		AXS_ERROR("%s:the Para is error!", __func__);
		return -EPERM;
	}
	ret = axs_write_bytes(write_reg,5);
	if (ret < 0) {
		AXS_ERROR("write diff cmd fail");
 		return -EPERM;
    }
	usleep_range(3000, 3100);

	ret = axs_read_bytes(g_axs_data->debug_rx_buf, rows * cols * 2);
	if (ret < 0) {
		AXS_ERROR("read diffdata/rawdata fail");
		goto out;
	}
	mutex_lock(&s_device_mutex);
	if (test_type == RAWDATA_TEST) {
		for (i = 0, index = 0; index < rows * cols; i += 2, index++) {
			frame_data_words[index] = (u16)((g_axs_data->debug_rx_buf[i] << 8) + g_axs_data->debug_rx_buf[i + 1]) / 10;
		}
	} else {
		for (i = 0, index = 0; index < rows * cols; i += 2, index++) {
			frame_data_words[index] = (g_axs_data->debug_rx_buf[i] << 8) + g_axs_data->debug_rx_buf[i + 1];
		}
	}
	for (x = 0; x < rows; x++) {
		pr_cont("AXS_CTP[%2d]", (x + 1));
		for (y = 0; y < cols; y++) {
			pr_cont("%5d,",  frame_data_words[x * cols + y]);
		}
		pr_cont("\n");
	}
	mutex_unlock(&s_device_mutex);
out:
	return ret;
}

int axs_enter_rawdata_mode(void)
{
	int ret = 0;

	u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_RAWDATA_READ}; // read tx,rx

	AXS_FUNC_ENTER();
	ret = axs_write_bytes(write_reg, 5);
	if (ret < 0) {
		AXS_ERROR("write diff cmd fail");
    }
	msleep(20);
	return ret;
}

static int axs_enter_normal_mode(void)
{
	int ret = 0;

	u8 write_reg[5] = {0x13, 0x31, 0x41, 0x51, 0x01}; // read tx,rx

	AXS_FUNC_ENTER();
	ret = axs_write_bytes(write_reg, 5);
	if (ret < 0) {
		AXS_ERROR("enter normal fail");
    }
	return ret;
}

static int  axs_testing_delta_raw_report(struct ztp_device *cdev, unsigned int num_of_reports)
{
	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0;
	int retval = 0;
	int len = 0;
	int i = 0;
	struct axs_ts_data *ts_data = g_axs_data;

	if (ts_data->suspended || !tpd_cdev->fw_ready) {
		AXS_ERROR("%s:In suspended", __func__);
		return -EIO;
	}

	row = RX_NUM;
	col = TX_NUM;
#if AXS_ESD_CHECK_EN
	axs_esd_check_suspend();
#endif
	axs_irq_disable();
	frame_data_words = kcalloc((row * col), sizeof(s16), GFP_KERNEL);
	if (frame_data_words ==  NULL) {
		AXS_ERROR("Failed to allocate frame_data_words mem");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	retval = axs_enter_rawdata_mode();
	if (retval < 0){
		goto DATA_REQUEST_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"frame: %d, TX:%d  RX:%d\n", idx, row, col);

		retval = axs_data_request(cdev, frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			AXS_ERROR("data_request failed!");
			goto DATA_REQUEST_FAILED;
		}
		len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"RawData:\n");
		for (i = 0; i < row * col; i++) {
			len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"%5d,", frame_data_words[i]);
			if ((i + 1) % col == 0)
				len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len, "\n");
		}
		retval = axs_data_request(cdev, frame_data_words, DELTA_TEST);
		if (retval < 0) {
			AXS_ERROR("data_request failed!");
			goto DATA_REQUEST_FAILED;
		}
		len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"DiffData:\n");
		for (i = 0; i < row * col; i++) {
			len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"%5d,", frame_data_words[i]);
			if ((i + 1) % col == 0)
				len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len, "\n");
		}
		usleep_range(5000, 6000);
	}
DATA_REQUEST_FAILED:
	msleep(20);
	axs_enter_normal_mode();
	AXS_INFO("get tp delta raw data end!");
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	axs_irq_enable();
#if AXS_ESD_CHECK_EN
	axs_esd_check_resume();
#endif
	return retval;
}

static int axs_get_noise(struct ztp_device *cdev)
{
	int ret =0;

	if(tp_alloc_tp_firmware_data(10 * RT_DATA_LEN)) {
		AXS_ERROR(" alloc tp firmware data fail");
		return -ENOMEM;
	}
	ret = axs_testing_delta_raw_report(cdev, 5);
	if (ret) {
		AXS_ERROR( "%s:get_noise failed\n",  __func__);
		tpd_zlog_record_notify(TP_GET_NOISE_ERROR_NO);
		return ret;
	} else {
		AXS_INFO("%s:get_noise success\n",  __func__);
	}
	return 0;
}


static int axs_get_self_test_result(struct ztp_device *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;

	AXS_FUNC_ENTER();

	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", axs_tp_test_result, RX_NUM,
		TX_NUM, 0);
	AXS_INFO("%s:tpd test:%s", __func__, buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int axs_self_test(struct ztp_device *cdev)
{
	int ret;
	struct axs_ts_data *ts_data = g_axs_data;
	int retry = 0;

	AXS_FUNC_ENTER();
	if (ts_data->suspended || !tpd_cdev->fw_ready) {
		AXS_INFO("%s:In suspended", __func__);
		return -EIO;
	}

#if AXS_ESD_CHECK_EN
	axs_esd_check_suspend();
#endif

	do {
		axs_irq_disable();
		axs_release_all_finger();
		axs_tp_test_result = 0;
		ret = axs_test_entry(axs_ini_filename);
		if (ret) {
			retry++;
			AXS_ERROR("tp self test fail,retry:%d!", retry);
			axs_fw_download();
			msleep(1000);
		} else {
			break;
		}
	} while (retry < 3);
	if (!axs_fw_download()) {
		AXS_ERROR("axs_download_init fail!\n");
	}
	/* axs_irq_enable(); */
#if AXS_ESD_CHECK_EN
	axs_esd_check_resume();
#endif
	if (axs_tp_test_result) {
		tpd_zlog_record_notify(TP_SELF_TEST_ERROR_NO);
	}
	if (ret) {
		AXS_INFO("axs selftest fail");
		return ret;
	}
	return ret;
}

static int tpd_cts_shutdown(struct ztp_device *cdev)
{
	struct axs_ts_data *ts_data = g_axs_data;

	axs_ts_suspend(ts_data->dev);
	return 0;
}

int tpd_ex_mode_recovery(struct ztp_device *cdev)
{
	AXS_INFO("%s enter", __func__);
#ifdef AXS_HEADSET_EN
	if (g_axs_data->headset_enable) {
		axs_headset_enable(g_axs_data->headset_enable);
	}
#endif

#if AXS_CHARGE_EN
	if (g_axs_data->charge_enable) {
		axs_charger_enable(g_axs_data->charge_enable);
	}
#endif
#ifdef AXS_PROXIMITY_SENSOR_EN
	AXS_DEBUG("psensor_state:%d, psensor_enable:%d\n", g_axs_data->psensor_state, g_axs_data->psensor_enable);
	if(g_axs_data->psensor_enable == 1) {
		AXS_INFO("set proximity mode enable:%d", g_axs_data->psensor_enable);
		axs_write_psensor_enable(g_axs_data->psensor_enable);
		g_axs_data->psensor_state = 0;
	}
#endif
	return 0;
}

static bool axs_suspend_need_awake(struct ztp_device *cdev)
{
	struct axs_ts_data *ts_data = (struct axs_ts_data *)cdev->tp_data;
	AXS_INFO("%s enter", __func__);

#ifdef AXS_PROXIMITY_SENSOR_EN
	if (ts_data->psensor_state && ts_data->psensor_enable) {
		AXS_INFO("tp suspend need awake");
		return true;
	} else {
		AXS_INFO("tp suspend dont need awake");
		ts_data->psensor_enable = 0;
		AXS_INFO("psensor_enable is %d", ts_data->psensor_enable);
		return false;
	}
#else
	AXS_INFO("tp suspend dont need awake");
	return false;
#endif
}

int axs_reset_test_val(void)
{
	u8 cmd_reset_test[5] = {0x13, 0x31, 0x41, 0x51, 0x1a};
	u8 val[2] = { 0 };
	int ret = 0;

	ret = axs_write_bytes(cmd_reset_test, sizeof(cmd_reset_test));
	if (ret) {
		AXS_ERROR("write tp reset test cmd fail");
		return ret;
	}
	usleep_range(1000, 1100);
	ret = axs_read_bytes(val, sizeof(val));
	if (ret) {
		AXS_ERROR("read tp reset test val fail");
 		return ret;
	}

	ret = val[0]<<8 | val[1];
	AXS_INFO("get tp reset test ret = 0x%x", ret);
	return ret;
}

static int axs_bbat_test(struct ztp_device *cdev)
{
	u8 cmd_int_test[5] = {0x13, 0x31, 0x41, 0x51, 0x0a};
	struct axs_ts_platform_data *pdata = g_axs_data->pdata;
	int ret = 0;

	AXS_INFO("%s enter", __func__);

#if AXS_ESD_CHECK_EN
	axs_esd_check_suspend();
#endif
/*tp int test*/
	cdev->bbat_test_enter = true;
	cdev->bbat_int_test = false;
	cdev->bbat_test_result = 0;
	reinit_completion(&cdev->bbat_test_completion);
	mutex_lock(&s_device_mutex);
	axs_write_buf(cmd_int_test, sizeof(cmd_int_test));
	mutex_unlock(&s_device_mutex);
	msleep(20);
	if (cdev->bbat_int_test ==  false) {
		ret = wait_for_completion_timeout(&cdev->bbat_test_completion, msecs_to_jiffies(700));
		if (!ret) {
			AXS_ERROR("tp int test fail");
			cdev->bbat_test_result = TP_INT_BAAT_TEST_FAIL;
		}
	}
/* tp rest test*/
	gpio_direction_output(pdata->reset_gpio, 1);
	msleep(20);
	if (axs_reset_test_val() != 0x5AA5){
		cdev->bbat_test_result = cdev->bbat_test_result | TP_RST_BAAT_TEST_FAIL;
	}
	gpio_set_value(pdata->reset_gpio, 0);
	msleep(20);
	if (axs_reset_test_val() == 0x5AA5){
		cdev->bbat_test_result = cdev->bbat_test_result | TP_RST_BAAT_TEST_FAIL;
	}
	gpio_set_value(pdata->reset_gpio, 1);
	msleep(200);
	if (!axs_fw_download()) {
		AXS_ERROR("axs_download_init fail!\n");
	}
	msleep(200);
#if AXS_ESD_CHECK_EN
	axs_esd_check_resume();
#endif
	cdev->bbat_test_enter = false;
	return cdev->bbat_test_result;
}

void axs_tpd_register_fw_class(void)
{
	AXS_FUNC_ENTER();
	tpd_cdev->get_tpinfo = tpd_init_tpinfo;
	tpd_cdev->tp_data = g_axs_data;
	tpd_cdev->tp_resume_func = axs_tp_resume;
	tpd_cdev->tp_suspend_func = axs_tp_suspend;
	tpd_cdev->tp_suspend_show = axs_get_tp_suspend;
	tpd_cdev->set_tp_suspend = axs_set_tp_suspend;
#ifdef AXS_HEADSET_EN
	tpd_cdev->headset_state_show = axs_get_headset_state;
	tpd_cdev->set_headset_state = axs_set_headset_state;
#endif
#if AXS_CHARGE_EN
	tpd_cdev->charger_state_notify = axs_charger_state_notify;
	queue_delayed_work(tpd_cdev->tpd_wq, &tpd_cdev->charger_work, msecs_to_jiffies(5000));
#endif
#ifdef AXS_ROTATION_EN
	tpd_cdev->set_display_rotation = axs_set_display_rotation;
#endif
	tpd_cdev->tp_fw_upgrade = axs_fw_upgrade;
	tpd_cdev->get_noise = axs_get_noise;
	tpd_cdev->get_tp_self_test_result = axs_get_self_test_result;
	tpd_cdev->tp_self_test = axs_self_test;
	tpd_cdev->tpd_shutdown = tpd_cts_shutdown;
	tpd_cdev->tpd_send_cmd = tpd_ex_mode_recovery;
	tpd_cdev->max_x = SCREEN_MAX_X;
	tpd_cdev->max_y = SCREEN_MAX_Y;
	tpd_cdev->input = g_axs_data->input_dev;
	tpd_cdev->tpd_suspend_need_awake = axs_suspend_need_awake;
	tpd_cdev->tp_bbat_test = axs_bbat_test;
	tpd_cdev->TP_have_registered = true;
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	zlog_tp_dev.device_name = axs_vendor_name;
	zlog_tp_dev.ic_name = "axs_tp";
	TPD_ZLOG("device_name:%s, ic_name: %s.", zlog_tp_dev.device_name, zlog_tp_dev.ic_name);
#endif
}
