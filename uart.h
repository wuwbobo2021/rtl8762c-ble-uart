// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>
#include <stdbool.h>

#include <app_msg.h>

#define Uart_Rx_Buf_Size 4096

// export
extern uint32_t uart_baudrate_target, uart_baudrate_actual;
extern volatile bool flag_rx_data_available;
extern volatile uint16_t uart_rx_count;
extern char uart_rx_buf[];

void board_uart_init(void);
bool driver_uart_init(uint32_t baudrate);
#if F_BT_DLPS_EN
    void uart_config_dlps(bool enter_dlps);
#endif

bool uart_senddata_continuous(const uint8_t *p_send, uint16_t cnt_send);
void uart_clear_rx_buffer(void);

// import
extern bool send_msg_to_app(T_IO_MSG *p_msg);

#endif
