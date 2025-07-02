#ifndef _AXS_TEST_H
#define _AXS_TEST_H

#include "axs_test_ini.h"

#define MAX_TEST_ITEM			32
#define MAX_TEST_ITEM_NAME_LEN	32

#define ERROR_RAWDATA	0x01
#define ERROR_OPEN		0x02
#define ERROR_SHORT		0x04

#define TXT_BUFFER_LEN                          (524288)

#define AXS_TEST_SAVE_INFO(fmt, args...) do { \
    if (axs_test->testresult) { \
        axs_test->testresult_len += snprintf( \
        axs_test->testresult + axs_test->testresult_len, \
        TXT_BUFFER_LEN - axs_test->testresult_len, \
        fmt, ##args);\
    } \
} while (0)

enum test_final_result {
    TEST_RESULT_INIT,
    TEST_RESULT_IN_PROGRESS,
    TEST_RESULT_PASS,
    TEST_RESULT_FAIL,
};

enum itemcode {
	//CODE_ENTER_FACTORY_MODE = 0,
	CODE_RAWDATA_TEST = 7,
	CODE_OPEN_TEST = 12,
	CODE_SHORT_TEST = 15,
	//CODE_CB_TEST = 25,
	//CODE_LCD_NOISE_TEST = 27,
	//CODE_MUX_OPEN_TEST = 41,
};

struct test_thr {
	int rawdata_test_min;
	int rawdata_test_max;
	int open_test_min;
	int open_test_max;
	int short_test_min;
	int short_test_max;
};

struct test_buf {
	char name[MAX_TEST_ITEM_NAME_LEN];
	int code;
	u16 *data;
	int datalen;
	int result;
};

struct test_data {
	struct test_buf buf[MAX_TEST_ITEM];
	int item_count;

	struct test_thr thr;
	int thr_count;
	int result;		//final result
};

struct host_test {
	struct ini_data ini;
	struct test_data test;

	int rx_num;
	int tx_num;
	int node;
	int *invalid_node;			//default 1 invalid 0

	int buffer_length;
	u8 *tmp_buffer;
	int test_error_item;	//bit0:rawdata error	bit1:open error		bit2:short error

	char *testresult;
	int testresult_len;
};

struct test_result {
	int test_error_item;	//bit0:rawdata error	bit1:open error		bit2:short error
	int result;		//final result
	int item_count;

	struct test_buf buf[MAX_TEST_ITEM];
};
//int axs_self_test_init(struct axs_ts_data *ts_data);
bool compare_array(u16 *data, int min, int max);
void show_data(u16 *data, bool key);

extern struct host_test *axs_test;
extern struct test_result *axs_test_result;		//save the final result
extern int axs15260_get_channel(struct host_test *tdata);
extern int axs_test_entry(char *ini_file_name);

#endif

