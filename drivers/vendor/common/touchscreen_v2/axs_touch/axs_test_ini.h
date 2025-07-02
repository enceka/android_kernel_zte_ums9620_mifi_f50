#ifndef _AXS_TEST_INI_H
#define	_AXS_TEST_INI_H

#define 	MAX_KEYWORD_NUM         20
#define		MAX_INI_KEYWORD_LEN		50
#define		MAX_INI_VALUE_LEN		512
#define 	MAX_INI_LINE_LEN		(MAX_INI_KEYWORD_LEN + MAX_INI_VALUE_LEN)

#define		MAX_IC_NAME_LEN         32

#define 	MAX_INI_SECTION_NUM     20

#define 	TEST_ITEM_COUNT_MAX     32

#define TEST_ITEM_INCELL            { \
    "RAWDATA_TEST", \
    "OPEN_CIRCUIT_TEST", \
    "SHORT_CIRCUIT_TEST", \
}

#define BASIC_THRESHOLD_INCELL      { \
    "RAWDATA_TEST_MIN", "RAWDATA_TEST_MAX", \
    "OPEN_CIRCUIT_TEST_MIN", "OPEN_CIRCUIT_TEST_MAX",  \
    "SHORT_CIRCUIT_TEST_MIN", "SHORT_CIRCUIT_TEST_MAX", \
}

enum line_type {
    LINE_SECTION = 1,
    LINE_KEYWORD = 2 ,
    LINE_OTHER = 3,
};

struct ini_keyword {
    char name[MAX_INI_KEYWORD_LEN];
    char value[MAX_INI_VALUE_LEN];
};

struct ini_section {
    char name[MAX_INI_KEYWORD_LEN];
    int keyword_num;
    /* point to ini.tmp, don't need free */
    struct ini_keyword *keyword;
};

struct testitem {
	u32 rawdata_test	: 1;
	u32 open_test		: 1;
	u32	short_test		: 1;
};

struct ini_data {
	char *data;
    char ic_type[MAX_IC_NAME_LEN];

	int length;
    int section_num;
    int keyword_num_total;

    union {
        int tmp;
        struct testitem item;
    } u;

    struct ini_section section[MAX_INI_SECTION_NUM];
    struct ini_keyword *tmp;
};

int axs_get_test_parameter_from_ini(char *ini_name);

#endif
