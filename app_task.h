// based on the ble_peripheral example in RTL8762C-sdk-gcc-v0.0.5.zip
// <https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a>
// modified by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

#ifndef _APP_TASK_H_
#define _APP_TASK_H_

#include <stdbool.h>
#include <app_msg.h>

extern void driver_init(void);

/**
 * @brief  Initialize App task
 * @return void
 */
void app_task_init(void);

bool send_msg_to_app(T_IO_MSG *p_msg);

#endif

