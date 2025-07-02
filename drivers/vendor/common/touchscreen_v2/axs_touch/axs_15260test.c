#include "axs_core.h"
#include "axs_platform.h"
#include "axs_test.h"
#include "axs_test_ini.h"

extern int axs_tp_test_result;

int axs15260_get_channel(struct host_test *tdata)
{
	tdata->tx_num = RX_NUM;
	tdata->rx_num = TX_NUM;

	return 0;
}

static int axs15260_read_rawdata(struct host_test *tdata, u8 *rawdata)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_RAWDATA_READ}; // read tx,rx
	int tx = tdata->tx_num;
	int rx = tdata->rx_num;

    ret = axs_write_bytes(write_reg, 5);
    if (ret) {
        AXS_ERROR("write rawdata cmd fail");
        return ret;
    }
    usleep_range(10000,11000);
    ret = axs_read_bytes(rawdata, tx*rx*2);
    if (ret < 0) {
        AXS_ERROR("read rawdata fail");
        return -EPERM;
    }

	return ret;
}

static void axs_raw_u8_to_u16(u8 *rawdata_u8, u16 *rawdata_u16, int byte_num)
{
	int i = 0;

    for (i = 0; i < byte_num; i = i + 2) {
        rawdata_u16[i >> 1] = (u16)((rawdata_u8[i] << 8) + rawdata_u8[i + 1]);
    }
}

static int axs15260_rawdata_test(struct host_test *tdata, bool *test_result)
{
    int ret = 0;
    bool tmp_result = false;
    u8 *rawdata = NULL;
	u16 *rawdata_u16 = NULL;
	struct host_test *test = tdata;

    AXS_FUNC_ENTER();
    AXS_TEST_SAVE_INFO("\n============ Test Item: RawData Test\n");

    rawdata = test->tmp_buffer;

    ret = axs15260_read_rawdata(test, rawdata);
    if (ret < 0) {
        AXS_ERROR("read rawdata fail,ret=%d\n", ret);
        goto test_err;
    }

	rawdata_u16 = kzalloc(test->node * sizeof(u16), GFP_KERNEL);
	if(rawdata_u16 == NULL) {
		AXS_ERROR("rawdata_u16 data buffer malloc fail\n");
		return -ENOMEM;
	}

	axs_raw_u8_to_u16(rawdata, rawdata_u16, test->node*2);

    tmp_result = compare_array(rawdata_u16,
                               test->test.thr.rawdata_test_min,
                               test->test.thr.rawdata_test_max);
	show_data(rawdata_u16, false);

    ret = 0;

test_err:
    if (tmp_result) {
        *test_result = true;
        AXS_TEST_SAVE_INFO("------ RawData Test PASS\n");
    } else {
        *test_result = false;
        AXS_TEST_SAVE_INFO("------ RawData Test NG\n");
    }

	kfree_safe(rawdata_u16);
    AXS_FUNC_EXIT();
    return ret;
}

static int axs15260_read_open_test_result(struct host_test *tdata, u8 *rawdata)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_OPEN_TEST_READ}; // read tx,rx
	int tx = tdata->tx_num;
	int rx = tdata->rx_num;

    ret = axs_write_bytes(write_reg, 5);
    if (ret) {
        AXS_ERROR("write read_open cmd fail");
        return ret;
    }
    usleep_range(10000,11000);
    ret = axs_read_bytes(rawdata, tx*rx*2);
    if (ret < 0) {
        AXS_ERROR("read rawdata fail");
        return -EPERM;
    }

	return ret;
}

static int axs15260_open_test_en(void)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_OPEN_TEST}; // read tx,rx

    ret = axs_write_bytes(write_reg,5);
    if (ret < 0)
    {
        AXS_ERROR("write fail");
        return -EPERM;
    }
	msleep(200);

	return ret;
}

static int axs15260_open_test(struct host_test *tdata, bool *test_result)
{
    int ret = 0;
    bool tmp_result = false;
    u8 *rawdata = NULL;
	u16 *rawdata_u16 = NULL;
	struct host_test *test = tdata;

    AXS_FUNC_ENTER();
    AXS_INFO("\n============ Test Item: OpenCircuit Test\n");

    memset(test->tmp_buffer, 0, test->buffer_length);
    rawdata = test->tmp_buffer;

    ret = axs15260_open_test_en();
    if (ret < 0) {
        AXS_ERROR("write open fail,ret=%d\n", ret);
        goto test_err;
    }

	ret = axs15260_read_open_test_result(test, rawdata);
    if (ret < 0) {
        AXS_ERROR("read rawdata fail,ret=%d\n", ret);
        goto test_err;
    }

	rawdata_u16 = kzalloc(test->node * sizeof(u16), GFP_KERNEL);
	if(rawdata_u16 == NULL) {
		AXS_ERROR("rawdata_u16 data buffer malloc fail\n");
		return -ENOMEM;
	}

	axs_raw_u8_to_u16(rawdata, rawdata_u16, test->node * 2);


    tmp_result = compare_array(rawdata_u16,
                               test->test.thr.open_test_min,
                               test->test.thr.open_test_max);

	show_data(rawdata_u16, false);

    ret = 0;

test_err:
    if (tmp_result) {
        *test_result = true;
        AXS_TEST_SAVE_INFO("------ OpenCircuit Test PASS\n");
    } else {
        *test_result = false;
        AXS_TEST_SAVE_INFO("------ OpenCircuit Test NG\n");
    }

	kfree_safe(rawdata_u16);
    AXS_FUNC_EXIT();
    return ret;
}

static int axs15260_read_short_test_result(struct host_test *tdata, u8 *rawdata)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_SHORT_TEST_READ}; // read tx,rx
	int tx = tdata->tx_num;
	int rx = tdata->rx_num;

    ret = axs_write_bytes(write_reg, 5);
    if (ret) {
        AXS_ERROR("write read_shot cmd fail");
        return ret;
    }
    usleep_range(10000,11000);
    ret = axs_read_bytes(rawdata, tx*rx*2);
    if (ret < 0) {
        AXS_ERROR("read rawdata fail");
        return -EPERM;
    }
	return ret;
}

static int axs15260_short_test_en(void)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_SHORT_TEST}; // read tx,rx

    ret = axs_write_bytes(write_reg,5);
    if (ret < 0)
    {
        AXS_ERROR("write fail");
        return -EPERM;
    }
	ssleep(4);

	return ret;
}

static int axs15260_short_test_exit(void)
{
    int ret = 0;
    u8 write_reg[5] = {0x13,0x31,0x41,0x51,AXS_FREG_SHORT_TEST_EXIT}; // read tx,rx

    ret = axs_write_bytes(write_reg,5);
    if (ret < 0)
    {
        AXS_ERROR("write fail");
        return -EPERM;
    }

	return ret;
}

static int axs15260_short_test(struct host_test *tdata, bool *test_result)
{
    int ret = 0;
    bool tmp_result = false;
    u8 *rawdata = NULL;
	u16 *rawdata_u16 = NULL;
	struct host_test *test = tdata;

    AXS_FUNC_ENTER();
    AXS_INFO("\n============ Test Item: Short Test\n");

    memset(test->tmp_buffer, 0, test->buffer_length);
    rawdata = test->tmp_buffer;

    ret = axs15260_short_test_en();
    if (ret < 0) {
        AXS_ERROR("write short fail,ret=%d\n", ret);
        goto test_err;
    }

	ret = axs15260_read_short_test_result(test, rawdata);
    if (ret < 0) {
        AXS_ERROR("read short fail,ret=%d\n", ret);
        goto test_err;
    }
    usleep_range(5000,5100);
    ret = axs15260_short_test_exit();
    if (ret < 0) {
        AXS_ERROR("short test exit fail,ret=%d\n", ret);
        goto test_err;
    }
	rawdata_u16 = kzalloc(test->node * sizeof(u16), GFP_KERNEL);
	if(rawdata_u16 == NULL) {
		AXS_ERROR("rawdata_u16 data buffer malloc fail\n");
		return -ENOMEM;
	}

	axs_raw_u8_to_u16(rawdata, rawdata_u16, test->node * 2);


    tmp_result = compare_array(rawdata_u16,
                               test->test.thr.short_test_min,
                               test->test.thr.short_test_max);

	show_data(rawdata_u16, false);

    ret = 0;

test_err:
    if (tmp_result) {
        *test_result = true;
        AXS_TEST_SAVE_INFO("------ ShortCircuit Test PASS\n");
    } else {
        *test_result = false;
        AXS_TEST_SAVE_INFO("------ ShortCircuit Test NG\n");
    }

	kfree_safe(rawdata_u16);
    AXS_FUNC_EXIT();
    return ret;
}

int start_test_axs15260(void)
{
    int ret = 0;
    struct host_test *tdata = axs_test;
    struct testitem *test_item = &tdata->ini.u.item;
    bool temp_result = false;
    bool test_result = true;

    AXS_FUNC_ENTER();
    AXS_INFO("test item:0x%x", axs_test->ini.u.tmp);
	tdata->test.item_count = 0;

    if (!tdata) {
        AXS_ERROR("tdata is null");
        return -EINVAL;
    }

	axs_enter_rawdata_mode();
    /* rawdata test */
	if (test_item->rawdata_test == true) {
        ret = axs15260_rawdata_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
			tdata->test_error_item = tdata->test_error_item | ERROR_RAWDATA;
            axs_tp_test_result = axs_tp_test_result | TP_RAWDATA_TEST_FAIL;
        }
    }
    usleep_range(5000, 5100);
    /* open test */
	if (test_item->open_test == true) {
        ret = axs15260_open_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
			tdata->test_error_item = tdata->test_error_item | ERROR_OPEN;
            axs_tp_test_result = axs_tp_test_result | TP_OPEN_TEST_FAIL;
        }
    }
    usleep_range(5000, 5100);
    /* short test */
	if (test_item->short_test == true) {
        ret = axs15260_short_test(tdata, &temp_result);
        if ((ret < 0) || (false == temp_result)) {
            test_result = false;
			tdata->test_error_item = tdata->test_error_item | ERROR_SHORT;
            axs_tp_test_result = axs_tp_test_result | TP_SHORT_TEST_FAIL;
        }
    }
    /*axs_enter_normal_mode();*/
    AXS_FUNC_EXIT();

    return test_result;
}


