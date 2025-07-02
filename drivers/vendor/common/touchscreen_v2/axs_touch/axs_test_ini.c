#include "axs_core.h"
#include "axs_platform.h"
#include "axs_test_ini.h"
#include "axs_test.h"

#define TOLOWER(x) ((x) | 0x20)

extern struct host_test *axs_test;

static int axs_strncmp(const char *cs, const char *ct, int count)
{
    u8 c1 = 0, c2 = 0;

    while (count) {
        if  ((*cs == '\0') || (*ct == '\0'))
			return -EPERM;
        c1 = (*cs++);
        c2 = (*ct++);
		if((c1 >= 'a') || (c1 <= 'z'))
			c1 = TOLOWER(c1);

		if((c2 >= 'a') || (c2 <= 'z'))
			c2 = TOLOWER(c2);

        if (c1 != c2)
            return c1 < c2 ? -1 : 1;
        if (!c1)
            break;
        count--;
    }

    return 0;
}

static int axs_isspace(int x)
{
    if (x == ' ' || x == '\t' || x == '\n' || x == '\f' || x == '\b' || x == '\r')
        return 1;
    else
        return 0;
}

static int axs_isdigit(int x)
{
    if (x <= '9' && x >= '0')
        return 1;
    else
        return 0;
}

static long axs_atol(char *nptr)
{
    int c; /* current char */
    long total; /* current total */
    int sign; /* if ''-'', then negative, otherwise positive */
    /* skip whitespace */
    while (axs_isspace((int)(unsigned char)*nptr))
        ++nptr;
    c = (int)(unsigned char) * nptr++;
    sign = c; /* save sign indication */
    if (c == '-' || c == '+')
        c = (int)(unsigned char) * nptr++; /* skip sign */
    total = 0;
    while (axs_isdigit(c)) {
        total = 10 * total + (c - '0'); /* accumulate digit */
        c = (int)(unsigned char) * nptr++; /* get next char */
    }
    if (sign == '-')
        return -total;
    else
        return total; /* return result, negated if necessary */
}

static int axs_atoi(char *nptr)
{
    return (int)axs_atol(nptr);
}

static void str_space_remove(char *str)
{
    char *t = str;
    char *s = str;

    while (*t != '\0') {
        if (*t != ' ') {
            *s = *t;
            s++;
        }
        t++;
    }

    *s = '\0';
}

static int ini_get_key(char *section_name, char *key_name, char *value)
{
    int i = 0;
    int j = 0;
    struct ini_data *ini = &axs_test->ini;
    struct ini_section *section;
    struct ini_keyword *keyword;
    int key_len = 0;

    for (i = 0; i < ini->section_num; i++) {
        section = &ini->section[i];
        key_len = strlen(section_name);
        if (key_len != strlen(section->name))
            continue;
        if (axs_strncmp(section->name, section_name, key_len) != 0)
            continue;

        for (j = 0; j < section->keyword_num; j++) {
            keyword = &section->keyword[j];
            key_len = strlen(key_name);
            if (key_len == strlen(keyword->name)) {
                if (0 == axs_strncmp(keyword->name, key_name, key_len)) {
                    key_len = strlen(keyword->value);
                    memcpy(value, keyword->value, key_len);

                    return key_len;
                }
            }
        }
    }

    return -ENODATA;
}

/* return keyword's value length if success */
static int axs_ini_get_string_value(char *section_name, char *key_name, char *rval)
{
    if (!section_name || !key_name || !rval) {
        AXS_ERROR("section_name/key_name/rval is null");
        return -EINVAL;
    }

    return ini_get_key(section_name, key_name, rval);
}

int axs_get_keyword_value(char *section, char *name, int *value)
{
    int ret = 0;
    char str[MAX_INI_VALUE_LEN] = { 0 };

    ret = axs_ini_get_string_value(section, name, str);
    if (ret > 0) {
        /* search successfully, so change value, otherwise keep default */
        *value = axs_atoi(str);
    }

    return ret;
}

static int axs_search_test_item(char name[][MAX_INI_KEYWORD_LEN], int length, int *val)
{
    int i = 0;
    int ret = 0;
    int tmpval = 0;

    if (length > TEST_ITEM_COUNT_MAX) {
        AXS_ERROR("test item count(%d) > max(%d)\n",
                          length, TEST_ITEM_COUNT_MAX);
        return -EINVAL;
    }

    AXS_INFO("test items in total of driver:%d", length);
    *val = 0;
    for (i = 0; i < length; i++) {
        tmpval = 0;
        ret = axs_get_keyword_value("TestItem", name[i], &tmpval);
        if (ret < 0) {
            AXS_INFO("test item:%s not found", name[i]);
        } else {
            AXS_INFO("test item:%s=%d", name[i], tmpval);
            *val |= (tmpval << i);
        }
    }

    return 0;
}

static int axs_get_test_item(void)
{
	int ret = 0;
    char item_name[][MAX_INI_KEYWORD_LEN] = TEST_ITEM_INCELL;
    int length = sizeof(item_name) / MAX_INI_KEYWORD_LEN;
    int item_val = 0;

    ret = axs_search_test_item(item_name, length, &item_val);
    if (ret < 0) {
        AXS_ERROR("get test item fail\n");
        return ret;
    }

    axs_test->ini.u.tmp = item_val;
    return 0;
}

static int axs_get_threshold_range(char name[][MAX_INI_KEYWORD_LEN], int length, int *val)
{
    int i = 0;
    int ret = 0;
    struct test_data *tdata = &axs_test->test;

    AXS_INFO("basic_thr string length(%d), count(%d)\n", length, tdata->thr_count);
    if (length > tdata->thr_count) {
        AXS_ERROR("basic_thr string length > count\n");
        return -EINVAL;
    }

    for (i = 0; i < length; i++) {
        ret = axs_get_keyword_value("TestParameter", name[i], &val[i]);
        if (ret < 0) {
            AXS_INFO("basic thr:%s not found", name[i]);
        } else {
            AXS_INFO("basic thr:%s=%d", name[i], val[i]);
        }
    }

    return 0;
}

static int axs_get_test_range(void)
{
    int ret = 0;
	char bthr_name_incell[][MAX_INI_KEYWORD_LEN] = BASIC_THRESHOLD_INCELL;
    int length = sizeof(bthr_name_incell) / MAX_INI_KEYWORD_LEN;
    struct test_data *tdata = &axs_test->test;

    AXS_FUNC_ENTER();
    tdata->thr_count = sizeof(struct test_thr) / sizeof(int);

    ret = axs_get_threshold_range(bthr_name_incell, length, (int *)&tdata->thr);
    if (ret < 0) {
        AXS_ERROR("get test thr range fail\n");
        return ret;
    }
    AXS_INFO("rawdata_test_min:%d, rawdata_test_max:%d",
        axs_test->test.thr.rawdata_test_min, axs_test->test.thr.rawdata_test_max);
    AXS_INFO("open_test_min:%d, open_test_max:%d",
        axs_test->test.thr.open_test_min, axs_test->test.thr.open_test_max);
    AXS_INFO("short_test_min:%d, short_test_max:%d",
        axs_test->test.thr.short_test_min, axs_test->test.thr.short_test_max);
    AXS_FUNC_EXIT();
	return 0;
}

static int axs_get_invalid_node(void)
{
	int ret = 0;
	int pos = 0;
	int tmp = 0;
    char str[MAX_INI_VALUE_LEN] = { 0 };
	struct host_test *tdata = axs_test;

	ret = axs_ini_get_string_value("TestParameter", "INVALID_NODE", str);
	if (ret < 0) {
		AXS_INFO("INVALID_NODE not found");
        return 0;
	} else {
		AXS_INFO("INVALID_NODE found, len = %d\nval = %s", ret, str);
	}

	while(str[pos] != '\0') {
		if(axs_isdigit(str[pos]))
			tmp = tmp * 10 + (str[pos] - '0');
		else if(',' == str[pos]) {
			tdata->invalid_node[tmp] = 0;
            tmp = 0;
		}
        pos++;
	}

	return ret;
}

static int axs_test_init(struct ini_data *ini)
{
	int ret = 0;
    //char str[MAX_INI_VALUE_LEN] = { 0 };

	/*IC type*/
    //axs_ini_get_string_value("Interface", "IC_Type", str);
    //snprintf(ini->ic_type, MAX_IC_NAME_LEN, "%s", str);
	//AXS_INFO("IC Type is %s", ini->ic_type);

	/*get test items*/
	ret = axs_get_test_item();
	if(ret < 0) {
		AXS_ERROR("get test item error");
		return ret;
	}
	/*get invalid node*/
	ret = axs_get_invalid_node();
	if(ret < 0) {
		AXS_ERROR("get invalid node error");
		return ret;
	}

	/*get test parameter*/
    ret = axs_get_test_range();
    if (ret < 0) {
        AXS_ERROR("get incell threshold fail\n");
        return ret;
    }

	/*AXS15260A*/
	return 0;

}

static void print_ini_data(struct ini_data *ini)
{
    int i = 0;
    int j = 0;
    struct ini_section *section = NULL;
    struct ini_keyword *keyword = NULL;

    if (!ini || !ini->tmp) {
        AXS_INFO("ini is null");
        return;
    }

    AXS_INFO("section num:%d, keyword num total:%d",
                 ini->section_num, ini->keyword_num_total);
    for (i = 0; i < ini->section_num; i++) {
        section = &ini->section[i];
        AXS_INFO("section name:[%s] keyword num:%d",
                     section->name, section->keyword_num);
        for (j = 0; j < section->keyword_num; j++) {
            keyword = &section->keyword[j];
            AXS_INFO("%s=%s", keyword->name, keyword->value);
        }
    }
}

static int axs_parse_ini_section(struct ini_data *ini, char *line_buffer)
{
    int length = strlen(line_buffer);
    struct ini_section *section = NULL;

    if ((length <= 2) || (length > MAX_INI_KEYWORD_LEN)) {
        AXS_ERROR("section line length fail");
        return -EINVAL;
    }

    if ((ini->section_num < 0) || (ini->section_num >= MAX_INI_SECTION_NUM)) {
        AXS_ERROR("section_num(%d) fail", ini->section_num);
        return -EINVAL;
    }
    section = &ini->section[ini->section_num];
    memcpy(section->name, line_buffer + 1, length - 2);
    section->name[length - 2] = '\0';
    AXS_INFO("section:%s, keyword offset:%d",
                  section->name, ini->keyword_num_total);
    section->keyword = (struct ini_keyword *)&ini->tmp[ini->keyword_num_total];
    section->keyword_num = 0;
    ini->section_num++;
	if (ini->section_num >= MAX_INI_SECTION_NUM) {
        AXS_ERROR("section num(%d)>max(%d), please check MAX_INI_SECTION_NUM",
                       ini->section_num, MAX_INI_SECTION_NUM);
        return -ENOMEM;
    }

    return 0;
}

static int axs_parse_ini_keyword(struct ini_data *ini, char *line_buffer)
{
    int i = 0;
    int offset = 0;
    int length = strlen(line_buffer);
    struct ini_section *section = NULL;

    for (i = 0; i < length; i++) {
        if (line_buffer[i] == '=')
            break;
    }

    if ((i == 0) || (i >= length)) {
        AXS_ERROR("mark(=)in keyword line fail");
        return -ENODATA;
    }

    if ((ini->section_num > 0) && (ini->section_num < MAX_INI_SECTION_NUM)) {
        section = &ini->section[ini->section_num - 1];
    }

    if (NULL == section) {
        AXS_ERROR("section is null");
        return -ENODATA;
    }

    offset = ini->keyword_num_total;
    if (offset > MAX_KEYWORD_NUM) {
        AXS_ERROR("keyword num(%d)>max(%d),please check MAX_KEYWORD_NUM",
                       ini->keyword_num_total, MAX_KEYWORD_NUM);
        return -ENODATA;
    }
    memcpy(ini->tmp[offset].name, &line_buffer[0], i);
    ini->tmp[offset].name[i] = '\0';
    memcpy(ini->tmp[offset].value, &line_buffer[i + 1], length - i - 1);
    ini->tmp[offset].value[length - i - 1] = '\0';
    section->keyword_num++;
    ini->keyword_num_total++;

    return 0;
}

static int axs_get_ini_line_data(char *filedata, char *line_data, int *len)
{
    int i = 0;
    int line_length = 0;
    int type;

    /* get a line data */
    for (i = 0; i < MAX_INI_LINE_LEN; i++) {
        if (('\n' == filedata[i]) || ('\r' == filedata[i])) {
            line_data[line_length++] = '\0';
            if (('\n' == filedata[i + 1]) || ('\r' == filedata[i + 1])) {
                line_length++;
            }
            break;
        } else {
            line_data[line_length++] = filedata[i];
        }
    }

    if (i >= MAX_INI_LINE_LEN) {
        AXS_ERROR("line length(%d)>max(%d)", line_length, MAX_INI_LINE_LEN);
        return -ENODATA;
    }

    /* remove space */
    str_space_remove(line_data);

    /* confirm line type */
    if (('\0' == line_data[0]) || ('#' == line_data[0])) {
        type = LINE_OTHER;
    } else if ('[' == line_data[0]) {
        type = LINE_SECTION;
    } else {
        type = LINE_KEYWORD; /* key word */
    }

    *len = line_length;
    return type;
}

static int axs_get_ini_data(struct ini_data* ini)
{
	int pos = 0;
	int ret = 0;
	char line_buffer[MAX_INI_LINE_LEN] = {0};
	int line_len = 0;

	if(!ini || !ini->data) {
		AXS_ERROR("ini is null\n");
		return -ENOMEM;
	}

	while(pos < ini->length) {
		/*get a line data*/
		ret = axs_get_ini_line_data(ini->data + pos, line_buffer, &line_len);

		if(ret < 0) {
			AXS_ERROR("get line data failed\n");
			return ret;
		} else if(ret == LINE_KEYWORD) {
			AXS_INFO("The current row is keyword\n");
			ret = axs_parse_ini_keyword(ini, line_buffer);
			if(ret < 0) {
				AXS_ERROR("parse keyword fail\n");
				return ret;
			}
		} else if(ret == LINE_SECTION) {
			AXS_INFO("The current row is section\n");
			ret = axs_parse_ini_section(ini, line_buffer);
			if(ret < 0) {
				AXS_ERROR("section keyword fail\n");
				return ret;
			}
		}

		pos += line_len;
	}

	print_ini_data(ini);

	return 0;
}

static int axs_test_get_ini_via_request_firmware(struct ini_data *ini, char *fwname)
{
    int ret = 0;
    const struct firmware *fw = NULL;
    struct device *dev = &g_axs_data->input_dev->dev;

	ret = request_firmware(&fw, fwname, dev);
	if (ret == 0) {
		AXS_INFO("firmware request(%s) success", fwname);
		ini->data = vmalloc(fw->size + 1);
		if (ini->data == NULL) {
			AXS_ERROR("ini->data buffer vmalloc fail");
			ret = -ENOMEM;
		} else {
			memcpy(ini->data, fw->data, fw->size);
			ini->data[fw->size] = '\n';
			ini->length = fw->size + 1;
		}
	} else {
		AXS_ERROR("firmware request(%s) fail,ret=%d", fwname, ret);
	}

    if (fw != NULL) {
        release_firmware(fw);
        fw = NULL;
    }

    return ret;
}

int axs_get_test_parameter_from_ini(char *ini_name)
{
	int ret = 0;
	struct ini_data *ini = &axs_test->ini;

    AXS_FUNC_ENTER();

	ret = axs_test_get_ini_via_request_firmware(ini, ini_name);
	if (ret != 0) {
		AXS_ERROR("get ini(default) fail");
		goto get_ini_err;
	}

    ini->keyword_num_total = 0;
    ini->section_num = 0;

    ini->tmp = vmalloc(sizeof(struct ini_keyword) * MAX_KEYWORD_NUM);
    if (NULL == ini->tmp) {
        AXS_ERROR("malloc memory for ini tmp fail");
        ret = -ENOMEM;
        goto get_ini_err;
    }
    memset(ini->tmp, 0, sizeof(struct ini_keyword) * MAX_KEYWORD_NUM);

    /* parse ini data to get keyword name&value */
	ret = axs_get_ini_data(ini);

	if(ret < 0) {
		AXS_ERROR("get ini data error, ret = %d\n", ret);
		return ret;
	}

    /* parse threshold & test item */
	ret = axs_test_init(ini);
    if (ret < 0) {
        AXS_ERROR("ini init fail");
        goto get_ini_err;
    }

	ret = 0;

get_ini_err:
	if (ini->tmp) {
		vfree(ini->tmp);
		ini->tmp = NULL;
	}

	if (ini->data) {
		vfree(ini->data);
		ini->data = NULL;
	}

    AXS_FUNC_EXIT();

	return ret;
}

