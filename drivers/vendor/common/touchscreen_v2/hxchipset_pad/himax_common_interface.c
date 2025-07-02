/************************************************************************
*
* File Name: himax_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include "himax_common.h"
#include "himax_ic_core.h"

extern int himax_chip_common_resume(struct himax_ts_data *ts);
extern int himax_chip_common_suspend(struct himax_ts_data *ts);
extern uint8_t HX_SMWP_EN;
#if defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH)
/* extern char *i_himax_firmware_name; */
#endif
#ifdef HX_USB_DETECT_GLOBAL
extern bool USB_detect_flag;
#endif
#ifdef HX_HOR_VER_SWITCH_MODE
extern int HOR_VER_SWITCH_detect_flag;
#endif

int himax_vendor_id = 0;
int himax_test_faied_buffer_length = 0;
int himax_test_failed_count = 0;
int himax_tptest_result = 0;
char himax_firmware_name[50] = {0};
char himax_mp_firmware_name[50] = {0};
char hx_criteria_csv_name[30] = {0};
#ifdef HIMAX_DEFAULT_FIRMWARE
char himax_default_firmware_name[50] = {0};
#endif
struct firmware *himax_adb_upgrade_firmware = NULL;

char *himax_test_failed_node_buffer = NULL;
char *himax_test_temp_buffer = NULL;
u8 *himax_test_failed_node = NULL;
bool	fw_updating = false;
#ifdef HXTP_DIFFER_CONFIG
bool himax_differ_config = false;
#endif

#define TEST_RESULT_LENGTH (8 * 1200)
#define TEST_TEMP_LENGTH 8
#ifdef HX_PINCTRL_EN
#define HIMAX_PINCTRL_INIT_STATE "pmx_ts_init"
#endif
#define TP_TEST_INIT		1
#define TP_TEST_START	2
#define TP_TEST_END		3

struct tpvendor_t himax_vendor_l[] = {
	{HX_VENDOR_ID_0, HXTS_VENDOR_0_NAME},
	{HX_VENDOR_ID_1, HXTS_VENDOR_1_NAME},
	{HX_VENDOR_ID_2, HXTS_VENDOR_2_NAME},
	{HX_VENDOR_ID_3, HXTS_VENDOR_3_NAME},
	{VENDOR_END, "Unknown"},
};

#ifdef HX_PINCTRL_EN
int himax_platform_pinctrl_init(struct himax_i2c_platform_data *pdata)
{
	int ret = 0;

	/* Get pinctrl if target uses pinctrl */
#ifdef CONFIG_TOUCHSCREEN_HIMAX_SPI
	pdata->ts_pinctrl = devm_pinctrl_get(&(hx_s_ts->spi->dev));
#else
	pdata->ts_pinctrl = devm_pinctrl_get(&(hx_s_ts->client->dev));
#endif
	if (IS_ERR_OR_NULL(pdata->ts_pinctrl)) {
		ret = PTR_ERR(pdata->ts_pinctrl);
		E("Target does not use pinctrl %d\n", ret);
		goto err_pinctrl_get;
	}

	pdata->pinctrl_state_init
	    = pinctrl_lookup_state(pdata->ts_pinctrl, HIMAX_PINCTRL_INIT_STATE);
	if (IS_ERR_OR_NULL(pdata->pinctrl_state_init)) {
		ret = PTR_ERR(pdata->pinctrl_state_init);
		E("Can not lookup %s pinstate %d\n", HIMAX_PINCTRL_INIT_STATE, ret);
		goto err_pinctrl_lookup;
	}

	ret = pinctrl_select_state(pdata->ts_pinctrl, pdata->pinctrl_state_init);
	if (ret < 0) {
		E("failed to select pin to init state");
		goto err_select_init_state;
	}

	return 0;

err_select_init_state:
err_pinctrl_lookup:
	devm_pinctrl_put(pdata->ts_pinctrl);
err_pinctrl_get:
	pdata->ts_pinctrl = NULL;
	return ret;
}
#endif

int himax_get_fw_by_lcdinfo(void)
{
	int i = 0;
	I("%s enter, lcd_name:%s", __func__, lcd_name);
	for (i = 0 ; i < (ARRAY_SIZE(himax_vendor_l) - 1) ; i++) {
		I("%s:%d--->%s", __func__, i, himax_vendor_l[i].vendor_name);
		if (strnstr(lcd_name, himax_vendor_l[i].vendor_name, strlen(lcd_name))) {
			I("%s:get_lcd_panel_name find", __func__);
			break;
		}
	}
	himax_vendor_id = himax_vendor_l[i].vendor_id;
	snprintf(himax_firmware_name, sizeof(himax_firmware_name),
		"Himax_firmware_%s.bin", himax_vendor_l[i].vendor_name);
	I("  himax_firmware_name:%s\n", himax_firmware_name);
#ifdef HIMAX_DEFAULT_FIRMWARE
	snprintf(himax_default_firmware_name, sizeof(himax_default_firmware_name),
		"%s_%s.bin", HIMAX_DEFAULT_FIRMWARE, himax_vendor_l[i].vendor_name);
	I("  himax_default_firmware_name:%s\n", himax_default_firmware_name);
#endif
	/* i_himax_firmware_name = himax_firmware_name; */
	snprintf(hx_criteria_csv_name, sizeof(hx_criteria_csv_name),
		"hx_criteria_%s.csv", himax_vendor_l[i].vendor_name);
	I("  hx_criteria_csv_name:%s\n", hx_criteria_csv_name);
	snprintf(himax_mp_firmware_name, sizeof(himax_mp_firmware_name),
		"Himax_mp_firmware_%s.bin", himax_vendor_l[i].vendor_name);
	I("  himax_mp_firmware_name:%s\n", himax_mp_firmware_name);
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	zlog_tp_dev.device_name = himax_vendor_l[i].vendor_name;
#endif
#ifdef HXTP_DIFFER_CONFIG
	if (strnstr(himax_vendor_l[i].vendor_name, HXTP_DIFFER_CONFIG, strlen(himax_vendor_l[i].vendor_name))) {
			I("%s:rx tx need differ config", __func__);
			himax_differ_config = true;
	}
#endif
	I("%s exit", __func__);
	return himax_vendor_l[i].vendor_id;
}

static int tpd_init_tpinfo(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	I("%s enter", __func__);

	if (ts->suspended) {
		I("%s:In suspended", __func__);
		return -EIO;
	}
	switch (himax_vendor_id) {
	case HX_VENDOR_ID_0:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_0_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_1:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_1_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_2:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_2_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	case HX_VENDOR_ID_3:
		strlcpy(cdev->ic_tpinfo.vendor_name, HXTS_VENDOR_3_NAME, sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	default:
		strlcpy(cdev->ic_tpinfo.vendor_name, "Unknown.", sizeof(cdev->ic_tpinfo.vendor_name));
		break;
	}
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "Himax_%s", ts->chip_name);
	cdev->ic_tpinfo.chip_model_id = TS_CHIP_HIMAX;
	cdev->ic_tpinfo.firmware_ver = hx_s_ic_data->vendor_fw_ver;
	if (hx_s_ts->chip_cell_type == CHIP_IS_ON_CELL) {
		cdev->ic_tpinfo.config_ver = hx_s_ic_data->vendor_config_ver;
	} else {
		cdev->ic_tpinfo.config_ver = hx_s_ic_data->vendor_touch_cfg_ver;
		cdev->ic_tpinfo.display_ver = hx_s_ic_data->vendor_display_cfg_ver;
	}
	cdev->ic_tpinfo.module_id = himax_vendor_id;

	return 0;
}

#ifdef HX_SMART_WAKEUP
static int tpd_get_wakegesture(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	I("%s wakeup_gesture_enable val is:%d.\n", __func__, ts->SMWP_enable);
	cdev->b_gesture_enable = ts->SMWP_enable;
	return cdev->b_gesture_enable;
}

static int tpd_enable_wakegesture(struct ztp_device *cdev, int enable)
{
	struct himax_ts_data *ts = hx_s_ts;

	ts->SMWP_enable = enable;
	ts->gesture_cust_en[0] = ts->SMWP_enable;
	if (!ts->suspended) {
		hx_s_core_fp._set_SMWP_enable(ts->SMWP_enable, ts->suspended);
		HX_SMWP_EN = ts->SMWP_enable;
	} else {
		cdev->tp_suspend_write_gesture = true;
		tpd_zlog_record_notify(TP_SUSPEND_GESTURE_OPEN_NO);
	}
	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, HX_SMWP_EN);
	return ts->SMWP_enable;
}
#endif

static bool himax_suspend_need_awake(struct ztp_device *cdev)
{
#ifdef HX_SMART_WAKEUP
	struct himax_ts_data *ts = hx_s_ts;

	if (!cdev->tp_suspend_write_gesture &&
		(fw_updating || ts->SMWP_enable)) {
		I("tp suspend need awake.\n");
		return true;
	}
#else
	if (fw_updating) {
		I("tp suspend need awake.\n");
		return true;
	}
#endif
	else {
		cdev->tp_suspend_write_gesture = false;
		I("tp suspend dont need awake.\n");
		return false;
	}
}


#ifdef HX_HIGH_SENSE
static int tpd_hsen_read(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	cdev->b_smart_cover_enable = ts->HSEN_enable;
	cdev->b_glove_enable = ts->HSEN_enable;

	return ts->HSEN_enable;
}

static int tpd_hsen_write(struct ztp_device *cdev, int enable)
{
	struct himax_ts_data *ts = hx_s_ts;

	ts->HSEN_enable = enable;
	if (!ts->suspended)
		g_core_fp.fp_set_HSEN_enable(ts->HSEN_enable, ts->suspended);
	I("%s: HSEN_enable = %d.\n", __func__, ts->HSEN_enable);

	return ts->HSEN_enable;
}
#endif

#ifdef HX_SENSIBILITY
static int himax_set_sensibility_level(struct ztp_device *cdev, u8 level)
{
	struct himax_ts_data *ts = hx_s_ts;

	ts->sensibility_level = level;
	cdev->sensibility_level = ts->sensibility_level;
	I("%s:sensibility = %d.\n", __func__, ts->sensibility_level);
	if (!ts->suspended) {
		if (g_core_fp.fp_set_sensibility_level(ts->sensibility_level) == true) {
			I("%s: sensibility_level write success/n", __func__);
		} else {
			E("%s: sensibility_level write fail/n", __func__);
		}
	}
	return ts->sensibility_level;
}
#endif

int himax_tp_requeset_firmware(void)
{
	struct ztp_device *cdev = tpd_cdev;

	if (cdev->tp_firmware == NULL || !cdev->tp_firmware->size) {
		E("cdev->tp_firmware is NULL");
		goto err_free_firmware;
	}

	if (himax_adb_upgrade_firmware) {
		kfree(himax_adb_upgrade_firmware);
		himax_adb_upgrade_firmware = NULL;
	}
	himax_adb_upgrade_firmware = kzalloc(sizeof(struct firmware), GFP_KERNEL);
	if (himax_adb_upgrade_firmware == NULL) {
		E("Request firmware alloc ts_firmware failed");
		return -ENOMEM;
	}

	himax_adb_upgrade_firmware->size = cdev->tp_firmware->size;
	himax_adb_upgrade_firmware->data = vmalloc(himax_adb_upgrade_firmware->size);
	if (himax_adb_upgrade_firmware->data == NULL) {
		E("Request form file alloc firmware data failed");
		goto err_free_firmware;
	}

	memcpy((u8 *)himax_adb_upgrade_firmware->data, (u8 *)cdev->tp_firmware->data, himax_adb_upgrade_firmware->size);
	return 0;
err_free_firmware:
	kfree(himax_adb_upgrade_firmware);
	himax_adb_upgrade_firmware = NULL;
	return -ENOMEM;
}

static int himax_tp_fw_upgrade(struct ztp_device *cdev, char *fw_name, int fwname_len)
{
	int result;

	I("%s: upgrade firmware from adb start !\n", __func__);

	/* manual upgrade will not use embedded firmware */
	if (himax_tp_requeset_firmware() < 0) {
		E("Get firmware from adb upgrade failed");
		return -EIO;
	}

	I("%s: FW image: %02X, %02X, %02X, %02X\n", __func__,
			himax_adb_upgrade_firmware->data[0], himax_adb_upgrade_firmware->data[1],
			himax_adb_upgrade_firmware->data[2], himax_adb_upgrade_firmware->data[3]);

	himax_int_enable(0);

#if defined(HX_ZERO_FLASH)
	I("NOW Running Zero flash update!\n");

	/* FW type: 0, normal; 1, MPFW */
	/* if (strcmp(fileName, MPAP_FWNAME) == 0)
		fw_type = 1; */

	CFG_TABLE_FLASH_ADDR = CFG_TABLE_FLASH_ADDR_T;
	hx_s_core_fp._bin_desc_get((unsigned char *)himax_adb_upgrade_firmware->data, HX1K);

	result = hx_s_core_fp._firmware_update_0f(himax_adb_upgrade_firmware, 0);
	if (result) {
		I("Zero flash update fail!\n");
	} else {
		I("Zero flash update complete!\n");
	}
#else
	I("NOW Running common flow update!\n");

	fw_type = (fw->size) / 1024;
	I("Now FW size is : %dk\n", fw_type);

	if (hx_s_core_fp._fts_ctpm_fw_upgrade_with_sys_fs(
	(unsigned char *)fw->data, fw->size, false) == 0) {
		E("%s: TP upgrade error, line: %d\n",
				__func__, __LINE__);
	} else {
		I("%s: TP upgrade OK, line: %d\n",
				__func__, __LINE__);
	}
#endif
	goto firmware_upgrade_done;
firmware_upgrade_done:
	hx_s_core_fp._reload_disable(0);
	hx_s_core_fp._power_on_init();
	hx_s_core_fp._read_FW_ver();

	hx_s_core_fp._tp_info_check();

	himax_int_enable(1);
	return result;
}

/* static int himax_gpio_shutdown_config(void)
{
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(hx_s_ts->rst_gpio)) {
		I("%s\n", __func__);
		gpio_set_value(hx_s_ts->rst_gpio, 0);
	}
#endif
	return 0;
} */

#ifdef HX_LCD_OPERATE_TP_RESET
static void himax_tp_reset_gpio_output(bool value)
{
	I("himax tp reset gpio set value: %d", value);
	gpio_direction_output(hx_s_ts->rst_gpio, value);
}
#endif

static int himax_ts_resume(void * himax_data)
{
	struct himax_ts_data *ts = (struct himax_ts_data *)himax_data;

	himax_chip_common_resume(ts);
	return 0;
}

static int himax_ts_suspend(void *himax_data)
{
	struct himax_ts_data *ts = (struct himax_ts_data *)himax_data;

	himax_chip_common_suspend(ts);
	return 0;
}

static int himax_tp_suspend_show(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	cdev->tp_suspend = ts->suspended;
	return cdev->tp_suspend;
}

static int himax_set_tp_suspend(struct ztp_device *cdev, u8 suspend_node, int enable)
{
	if (enable) {
		change_tp_state(LCD_OFF);
	} else {
		change_tp_state(LCD_ON);
	}
	return 0;
}

#ifdef HX_HOR_VER_SWITCH_MODE
static int himax_set_display_rotation(struct ztp_device *cdev, int mrotation)
{
	int ret = 0;
	struct himax_ts_data *ts = hx_s_ts;

	cdev->display_rotation = mrotation;
	HOR_VER_SWITCH_detect_flag = mrotation;
	if (ts->suspended)
	{
		I("%s:In suspended", __func__);
		return -EIO;
	}
	I("%s: display_rotation = %d.\n", __func__, cdev->display_rotation);
	switch (cdev->display_rotation) {
	case mRotatin_0:
		I("mRotatin_0 00");
		ret = hx_s_core_fp._set_hor_ver_switch_enable(mRotatin_0);
		break;
	case mRotatin_90:/*USB on right*/
		I("mRotatin_90 01");
		ret = hx_s_core_fp._set_hor_ver_switch_enable(mRotatin_90);
		break;
	case mRotatin_180:
		I("mRotatin_180 00");
		ret = hx_s_core_fp._set_hor_ver_switch_enable(mRotatin_180);
		break;
	case mRotatin_270:/*USB on left*/
		I("mRotatin_270 02");
		ret = hx_s_core_fp._set_hor_ver_switch_enable(mRotatin_270);
		break;
	default:
		break;
	}
	if (ret == 0) {
		I("%s: mrotation write success/n", __func__);
	} else {
		I("%s: mrotation write fail/n", __func__);
	}
	return cdev->display_rotation;
}
#endif

#ifdef HX_EDGE_LIMIT
static int himax_set_edge_limit_level(struct ztp_device *cdev, u8 level)
{
	int ret = 0;
	struct himax_ts_data *ts = hx_s_ts;

	ts->edge_limit_level = level;
	I("%s: edge limit level = %d.\n", __func__, level);
	if (ts->suspended)
		return 0;
	ret = g_core_fp.fp_edge_limit_level_set(level);
	if (ret == true) {
		I("%s: edge limit level write success/n", __func__);
	} else {
		I("%s: edge limit level write fail/n", __func__);
		return -EIO;
	}
	return level;
}
#endif

#ifdef HX_HEADPHONE
static int himax_headset_state_show(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	cdev->headset_state = ts->hp_en;
	return cdev->headset_state;
}

static int himax_set_headset_state(struct ztp_device *cdev, int enable)
{
	struct himax_ts_data *ts = hx_s_ts;

	ts->hp_en = enable;
	I("%s: headset_state = %d.\n", __func__, ts->hp_en);
	if (!ts->suspended)
		hx_s_core_fp._set_headphone_en(ts->hp_en, ts->suspended);
	else
		I("%s: tp is suspended, set headset fail.\n", __func__);
	return ts->hp_en;
}
#endif

#ifdef HX_USB_DETECT_GLOBAL
static int himax_charger_notify_call(struct ztp_device *cdev)
{
	struct himax_ts_data *ts = hx_s_ts;

	bool charger_mode_old = USB_detect_flag;

	USB_detect_flag = cdev->charger_mode;

	if(!ts->suspended && (USB_detect_flag != charger_mode_old)) {
		if(USB_detect_flag) {
			I("%s: charger in.\n", __func__);
			ts->cable_config[1] = 0x01;
			ts->usb_connected = 0x01;
		} else {
			I("%s: charger out.\n", __func__);
			ts->cable_config[1] = 0x00;
			ts->usb_connected = 0x00;
		}
		hx_s_core_fp._usb_detect_set(ts->cable_config);
	}
	return 0;
}
#endif

#ifdef HX_GET_NOISE
static int himax_print_data2buffer(struct ztp_device *cdev, s16 *frame_data_words, enum tp_test_type test_type, int *len)
{
	unsigned int row = 0;
	unsigned int col = 0;
	int i, j;

	row = hx_s_ic_data->rx_num;
	col = hx_s_ic_data->tx_num;

	switch (test_type) {
	case RAWDATA_TEST:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"RawData:\n");
		break;
	case DELTA_TEST:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"DiffData:\n");
		break;
	case BASE_TEST:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"BaseData:\n");
		break;
	default:
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"Unknow:\n");
	}

	for (j = 0; j < row; ++j) {
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
			"HXTP[%2d]", j + 1);
		for (i = 0; i < col; ++i) {
			*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len,
				"%5d,", frame_data_words[(row - 1 - j) + (i * row)]);
		}
		*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len, "\n");
	}
	*len += snprintf((char *)(cdev->tp_firmware->data + *len), RT_DATA_LEN * 10 - *len, "\n\n");
	return *len;
}

static int himax_data_request(s16 *frame_data_words, enum tp_test_type test_type)
{
	struct himax_ts_data *ts = hx_s_ts;
	int index = 0, ret = 0, i = 0;
	int diag_command = 0;
	int total_size = 0;
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int col = 0;
	unsigned int row = 0;

	row = hx_s_ic_data->rx_num;
	col = hx_s_ic_data->tx_num;
	total_size = (hx_s_ic_data->tx_num * hx_s_ic_data->rx_num) * 2;
	if (ts->noise_buffer == NULL) {
		ret = -ENOMEM;
		goto sub_end;
	}

	memset(ts->noise_buffer, 0, total_size);
	switch (test_type) {
	case RAWDATA_TEST:
		diag_command = 0x0A;
		break;
	case DELTA_TEST:
		diag_command = 0x09;
		break;
	case BASE_TEST:
		diag_command = 0x0B;
		break;
	default:
		E("%s:the Para is error!\n", __func__);
		ret = -1;
		goto sub_end;
	}
	hx_s_core_fp._diag_register_set(diag_command, 0, false);

	if (!hx_s_core_fp._get_DSRAM_data(ts->noise_buffer, 0)) {
		E("%s:request data error!\n", __func__);
		ret = -1;
	}
	diag_command = 0x00;
	hx_s_core_fp._diag_register_set(diag_command, 0, false);
	if (ret >= 0) {
		for (i = 0, index = 0; index < total_size/2; i += 2, index++) {
			frame_data_words[index] = ((ts->noise_buffer[i + 1] << 8) | ts->noise_buffer[i]);
		}
	}

	for (y = 0; y < row; ++y) {
		pr_cont("HXTP[%2d]", y + 1);
		for (x = 0; x < col; ++x) {
			pr_cont("%5d,", frame_data_words[(row - 1 - y) + (row * x)]);
		}
		pr_cont("\n");
	}

sub_end:
	return ret;
}

static int  himax_testing_delta_raw_report(struct ztp_device *cdev, u8 num_of_reports)
{
	s16 *frame_data_words = NULL;
	unsigned int col = 0;
	unsigned int row = 0;
	unsigned int idx = 0;
	int retval = 0;
	int len = 0;

	row = hx_s_ic_data->rx_num;
	col = hx_s_ic_data->tx_num;
	himax_int_enable(0);
	frame_data_words = kcalloc((row * col), sizeof(s16), GFP_KERNEL);
	if (frame_data_words ==  NULL) {
		E("Failed to allocate frame_data_words mem\n");
		retval = -1;
		goto MEM_ALLOC_FAILED;
	}
	for (idx = 0; idx < num_of_reports; idx++) {
		len += snprintf((char *)(cdev->tp_firmware->data + len), RT_DATA_LEN * 10 - len,
				"frame: %d, TX:%d  RX:%d\n", idx, col, row);
		retval = himax_data_request(frame_data_words, RAWDATA_TEST);
		if (retval < 0) {
			E("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = himax_print_data2buffer(cdev, frame_data_words, RAWDATA_TEST, &len);
		if (retval <= 0) {
			E("print_data2buffer rawdata failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		retval = himax_data_request(frame_data_words, DELTA_TEST);
		if (retval < 0) {
			E("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = himax_print_data2buffer(cdev, frame_data_words, DELTA_TEST, &len);
		if (retval <= 0) {
			E("print_data2buffer Delta failed!\n");
			goto DATA_REQUEST_FAILED;
		}

		retval = himax_data_request(frame_data_words, BASE_TEST);
		if (retval < 0) {
			E("data_request failed!\n");
			goto DATA_REQUEST_FAILED;
		}
		retval = himax_print_data2buffer(cdev, frame_data_words, BASE_TEST, &len);
		if (retval <= 0) {
			E("print_data2buffer Basedata failed!\n");
			goto DATA_REQUEST_FAILED;
		}
	}

	retval = 0;
	msleep(20);
	I("get tp delta raw data end!\n");
DATA_REQUEST_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;
MEM_ALLOC_FAILED:
	himax_int_enable(1);
	return retval;
}

static int himax_tpd_get_noise(struct ztp_device *cdev)
{
	int retval;
	int total_size = 0;
	struct himax_ts_data *ts = hx_s_ts;

	if (ts->suspended)
		return -EIO;

	if(tp_alloc_tp_firmware_data(10 * RT_DATA_LEN)) {
		E(" alloc tp firmware data fail");
		return -ENOMEM;
	}
	total_size = (hx_s_ic_data->tx_num * hx_s_ic_data->rx_num) * 2;
	if (ts->noise_buffer == NULL) {
		ts->noise_buffer = kzalloc(total_size, GFP_KERNEL);
		if (ts->noise_buffer == NULL) {
			E(" alloc ts->noise_buffer fail");
			return -ENOMEM;
		}
	}
	retval = himax_testing_delta_raw_report(cdev, 3);
	if (retval < 0) {
		E("%s: get_raw_noise failed!\n",  __func__);
		tpd_zlog_record_notify(TP_GET_NOISE_ERROR_NO);
		return retval;
	}
	return 0;
}
#endif

/* himax TP slef test*/
static int tpd_test_cmd_show(struct ztp_device *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	int i_len = 0;

	I("%s:enter\n", __func__);
	i_len = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", himax_tptest_result, hx_s_ic_data->tx_num,
		hx_s_ic_data->rx_num, himax_test_failed_count);

	I("tpd test:%s.\n", buf);
	num_read_chars = i_len;
	return num_read_chars;
}

static int tpd_test_cmd_store(struct ztp_device *cdev)
{
	himax_tptest_result = 0;

	I("%s: enter, %d\n", __func__, __LINE__);

	if (hx_s_ts->suspended == 1) {
		E("%s: please do self test in normal active mode\n", __func__);
		return HX_INIT_FAIL;
	}

	if (hx_s_ts->in_self_test == 1) {
		W("%s: Self test is running now!\n", __func__);
		return 0;
	}
	hx_s_ts->in_self_test = 1;

	himax_int_enable(0);

	hx_s_core_fp._chip_self_test(NULL, NULL);

#if defined(HX_EXCP_RECOVERY)
	HX_EXCP_RESET_ACTIVATE = 1;
#endif
	himax_int_enable(1);

	hx_s_ts->in_self_test = 0;
	if (himax_tptest_result) {
		tpd_zlog_record_notify(TP_SELF_TEST_ERROR_NO);
	}
	return 0;
}

#ifdef HX_PEN_ONLY_SWITCH
static int tpd_get_pen_only_mode(struct ztp_device *cdev)
{
	I(" pen only mode is %d",cdev->pen_only_mode);
	return cdev->pen_only_mode;
}

static int tpd_set_pen_only_mode(struct ztp_device *cdev, u8 pen_only_mode)
{
	uint32_t tmp_addr = 0x10007410;
	uint8_t tmp_data[DATA_LEN_4] ={0x5A,0xA5,0x5A,0xA5};
	uint8_t tmp_clear[DATA_LEN_4]={0x00,0x00,0x00,0x00};
	struct himax_ts_data *ts = hx_s_ts;
	int ret = 0;

	if (ts->suspended)
		return -EIO;
	cdev->pen_only_mode = pen_only_mode;
	I(" pen only mode is %d",cdev->pen_only_mode);
	if (pen_only_mode){
		ret = hx_s_core_fp._register_write(tmp_addr, tmp_data, 4);
	} else {
		ret = hx_s_core_fp._register_write(tmp_addr, tmp_clear, 4);
	}
	if (!ret)
		I("set pen only mode success");
	else
		E("set pen only  mode failed!");

	return 0;
}
#endif

void himax_tpd_register_fw_class(void)
{
	I("tpd_register_fw_class\n");

	tpd_cdev->get_tpinfo = tpd_init_tpinfo;
#ifdef HX_SMART_WAKEUP
	tpd_cdev->get_gesture = tpd_get_wakegesture;
	tpd_cdev->wake_gesture = tpd_enable_wakegesture;
#endif
#ifdef HX_HIGH_SENSE
	tpd_cdev->get_smart_cover = tpd_hsen_read;
	tpd_cdev->set_smart_cover = tpd_hsen_write;
	tpd_cdev->get_glove_mode = tpd_hsen_read;
	tpd_cdev->set_glove_mode = tpd_hsen_write;
#endif
	tpd_cdev->tp_fw_upgrade = himax_tp_fw_upgrade;
	/* tpd_cdev->tpd_gpio_shutdown = himax_gpio_shutdown_config; */
	tpd_cdev->tpd_suspend_need_awake = himax_suspend_need_awake;
	tpd_cdev->tp_suspend_show = himax_tp_suspend_show;
	tpd_cdev->set_tp_suspend = himax_set_tp_suspend;

#ifdef HX_USB_DETECT_GLOBAL
	tpd_cdev->charger_state_notify = himax_charger_notify_call;
	queue_delayed_work(tpd_cdev->tpd_wq, &tpd_cdev->charger_work, msecs_to_jiffies(5000));
#endif
#ifdef HX_SENSIBILITY
	tpd_cdev->set_sensibility = himax_set_sensibility_level;
	hx_s_ts->sensibility_level = 1;
#endif
#ifdef HX_LCD_OPERATE_TP_RESET
	tpd_cdev->tp_reset_gpio_output = himax_tp_reset_gpio_output;
#endif
#ifdef HX_HOR_VER_SWITCH_MODE
	tpd_cdev->set_display_rotation = himax_set_display_rotation;
#endif
#ifdef HX_HEADPHONE
	tpd_cdev->headset_state_show = himax_headset_state_show;
	tpd_cdev->set_headset_state = himax_set_headset_state;
#endif

	tpd_cdev->max_x = hx_s_ts->pdata->abs_x_max;
	tpd_cdev->max_y = hx_s_ts->pdata->abs_y_max;
	tpd_cdev->input = hx_s_ts->input_dev;
#ifdef HX_GET_NOISE
	tpd_cdev->get_noise = himax_tpd_get_noise;
#endif
	tpd_cdev->tp_data = hx_s_ts;
	tpd_cdev->tp_resume_func = himax_ts_resume;
	tpd_cdev->tp_suspend_func = himax_ts_suspend;
	tpd_cdev->tp_self_test = tpd_test_cmd_store;
	tpd_cdev->get_tp_self_test_result = tpd_test_cmd_show;
	tpd_cdev->tp_resume_before_lcd_cmd = true;
#ifdef HX_PEN_ONLY_SWITCH
	tpd_cdev->get_pen_only_mode = tpd_get_pen_only_mode;
	tpd_cdev->set_pen_only_mode = tpd_set_pen_only_mode;
#endif
	tpd_cdev->TP_have_registered = true;
#ifdef CONFIG_VENDOR_ZTE_LOG_EXCEPTION
	zlog_tp_dev.ic_name = "himax_tp";
	TPD_ZLOG("device_name:%s, ic_name: %s.", zlog_tp_dev.device_name, zlog_tp_dev.ic_name);
#endif
}
