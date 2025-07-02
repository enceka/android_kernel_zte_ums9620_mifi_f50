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
#if AXS_PROXIMITY_SENSOR_EN
void axs_report_psensor_state(u8 state)
{
	return;
}

int axs_read_psensor_enable(u8 *pEnable)
{
    u8 cmd_type[1] = {AXS_REG_PSENSOR_READ};
    return axs_read_regs(cmd_type, 1, pEnable, 1);
}
int axs_write_psensor_enable(u8 enable)
{
    u8 cmd_type[2] = {AXS_REG_PSENSOR_WRITE,enable};
    return axs_write_buf(cmd_type, 2);
}
#endif


