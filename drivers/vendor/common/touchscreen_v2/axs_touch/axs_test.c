#include "axs_core.h"
#include "axs_platform.h"
#include "axs_test.h"

struct host_test *axs_test = NULL;
struct test_result *axs_test_result = NULL;

extern int start_test_axs15260(void);

static int axs_test_save_test_data(char *data_buf, int len)
{
	AXS_FUNC_ENTER();
	tpd_copy_to_tp_firmware_data(data_buf);
	AXS_FUNC_EXIT();
	return 0;
}

static void axs_test_save_result_txt(struct host_test *tdata)
{
    if (!tdata || !tdata->testresult) {
        AXS_ERROR("test result is null");
        return;
    }

    AXS_INFO("test result length in txt:%d", tdata->testresult_len);
    axs_test_save_test_data(tdata->testresult,
                            tdata->testresult_len);
}

/*
 * show_data - show and save test data to testresult.txt
 */
void show_data(u16 *data, bool key)
{
    int i = 0;
    int j = 0;
    struct host_test *tdata = axs_test;
    int node_num = tdata->node;
    int tx_num = tdata->tx_num;
    int rx_num = tdata->rx_num;

    AXS_FUNC_ENTER();
    for (i = 0; i < tx_num; i++) {
        AXS_TEST_SAVE_INFO("Ch/Tx_%02d:  ", i + 1);
        for (j = 0; j < rx_num; j++) {
            AXS_TEST_SAVE_INFO("%5d, ", data[i * rx_num + j]);
        }
        AXS_TEST_SAVE_INFO("\n");
    }

    if (key) {
        AXS_TEST_SAVE_INFO("Ch/Tx_%02d:  ", tx_num + 1);
        for (i = tx_num * rx_num; i < node_num; i++) {
            AXS_TEST_SAVE_INFO("%5d, ",  data[i]);
        }
        AXS_TEST_SAVE_INFO("\n");
    }
    AXS_FUNC_EXIT();
}

bool compare_array(u16 *data, int min, int max)
{
    int i = 0;
    bool result = true;
    struct host_test *tdata = axs_test;
    int rx = tdata->rx_num;
    int node_num = tdata->node;

    if (!data || !tdata->invalid_node) {
        AXS_ERROR("data/invalid_node is null\n");
        return false;
    }

    for (i = 0; i < node_num; i++) {
        if (0 == tdata->invalid_node[i])
            continue;

        if ((data[i] < min) || (data[i] > max)) {
            AXS_ERROR("test fail,node(%4d,%4d)=%5d,range=(%5d,%5d)\n",
                              i / rx + 1, i % rx + 1, data[i], min, max);
            result = false;
        }
    }

    return result;
}

static int axs_test_start(void)
{
	return start_test_axs15260();
}

static int axs_get_test_parameter(char *ini_name)
{
	int ret = 0;

    AXS_FUNC_ENTER();

	ret = axs_get_test_parameter_from_ini(ini_name);

    AXS_FUNC_EXIT();

	return ret;
}

static int axs_malloc_testmem(void)
{
    int ret = 0;

    AXS_FUNC_ENTER();
    axs_test = kzalloc(sizeof(struct host_test), GFP_KERNEL);
    if (NULL == axs_test) {
        AXS_ERROR("malloc memory for test fail");
        return -ENOMEM;
    }
	g_axs_data->axs_test = axs_test;

    axs_test_result = kzalloc(sizeof(struct test_result), GFP_KERNEL);
    if (NULL == axs_test_result) {
        AXS_ERROR("malloc memory for test fail");
        return -ENOMEM;
    }

    axs_test->testresult = vmalloc(TXT_BUFFER_LEN);
	if (NULL == axs_test->testresult) {
		AXS_ERROR("tdata->testresult malloc fail\n");
		return -ENOMEM;
    }

	axs_test->test.result = TEST_RESULT_INIT;

    AXS_FUNC_EXIT();

    return ret;
}

static int axs_test_main_init(void)
{
    int ret = 0;
    struct host_test *tdata = NULL;

    AXS_FUNC_ENTER();

	ret = axs_malloc_testmem();
	if(ret < 0) {
        AXS_ERROR("malloc mem fail\n");
		return -ENOMEM;
	}
    tdata = axs_test;
    /* Init fts_test_data to 0 before test,  */
    tdata->test.result = TEST_RESULT_IN_PROGRESS;
    tdata->ini.u.tmp = 0xFFFFFFFF;		//default all test

	ret = axs15260_get_channel(tdata);
	if(ret < 0) {
        AXS_TEST_SAVE_INFO("get channle fail\n");
		return -1;
	}

	tdata->node = tdata->rx_num * tdata->tx_num;
	tdata->buffer_length = tdata->node * 2;
	AXS_INFO("rx = %d tx = %d buffer_length = %d", tdata->rx_num, tdata->tx_num, tdata->buffer_length);
	AXS_TEST_SAVE_INFO("rx = %d,tx = %d\n", tdata->rx_num, tdata->tx_num);

    tdata->tmp_buffer = kzalloc(tdata->buffer_length, GFP_KERNEL);
	if(tdata->tmp_buffer == NULL) {
		AXS_ERROR("test->tmp_buffer data buffer malloc fail\n");
		return -ENOMEM;
	}
    memset(tdata->tmp_buffer, 0, tdata->buffer_length);

	tdata->invalid_node = kzalloc(tdata->node, GFP_KERNEL);
	if(tdata->invalid_node == NULL) {
		AXS_ERROR("test->tmp_buffer data buffer malloc fail\n");
		return -ENOMEM;
	}
    memset(tdata->invalid_node, 1, tdata->node);
    AXS_FUNC_EXIT();
    return ret;
}

static int axs_test_main_exit(void)
{
 	int i = 0;

    AXS_FUNC_ENTER();

	for(i = 0; i < axs_test->test.item_count; i++) {
		kfree_safe(axs_test->test.buf[i].data);
	}

	kfree_safe(axs_test->tmp_buffer);
	vfree_safe(axs_test->testresult);
    kfree_safe(axs_test->invalid_node);
	kfree_safe(axs_test);
	g_axs_data->axs_test = NULL;
    AXS_FUNC_EXIT();
    return 0;
}

/*
 * axs_test_entry - test main entry
 *
 * warning - need disable irq & esdcheck before call this function
 *
 */
int axs_test_entry(char *ini_file_name)
{
    int ret = 0;
    int i = 0;

    /* test initialize */
    ret = axs_test_main_init();
    if (ret < 0) {
        AXS_ERROR("fts_test_main_init fail");
        axs_test->test.result = TEST_RESULT_FAIL;
        goto test_err;
    }

    /*Read parse configuration file*/
    AXS_INFO("ini_file_name:%s\n", ini_file_name);
    ret = axs_get_test_parameter(ini_file_name);
    if (ret < 0) {
        AXS_ERROR("get testparam fail");
        axs_test->test.result = TEST_RESULT_FAIL;
       goto test_err;
    }

    /* Start testing according to the test configuration */
    if (true == axs_test_start()) {
        AXS_TEST_SAVE_INFO("\n\n=======Tp test pass.\n");
        axs_test->test.result = TEST_RESULT_PASS;
         ret = 0;
    } else {
        AXS_TEST_SAVE_INFO("\n\n=======Tp test failure.\n");
        axs_test->test.result = TEST_RESULT_FAIL;
        ret = -EIO;
    }
    for (i= 0; i < axs_test->test.item_count; i++) {
        memcpy(&axs_test_result->buf[i], &axs_test->test.buf[i], sizeof(struct test_buf));
    }
	axs_test_result->test_error_item = axs_test->test_error_item;
	axs_test_result->result = axs_test->test.result;
	axs_test_result->item_count = axs_test->test.item_count;

	axs_test_save_result_txt(axs_test);
test_err:
    axs_test_main_exit();
    return ret;
}
