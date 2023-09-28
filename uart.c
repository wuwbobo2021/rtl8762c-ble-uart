// based on the UART example in RTL8762C-sdk-gcc-v0.0.5.zip
// <https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a>
// and the UART example in <https://github.com/cyber-murmel/rtl8762c-gcc-examples>
// restructured by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

#include "rtl876x.h"

#include <trace.h>
#include <os_sync.h>

#include "rtl876x_nvic.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_rcc.h"
#include "rtl876x_uart.h"

#include "uart.h"
#include "rtlbaud.h"
#include "board.h"

#ifndef UART0
	#define UART0 UART
#endif

// extern
uint32_t uart_baudrate_target = 9600, uart_baudrate_actual = 0;
volatile bool flag_rx_data_available = false;
volatile uint16_t uart_rx_count = 0;
char uart_rx_buf[Uart_Rx_Buf_Size];

static void* mutex_uart = (void*)0;

void board_uart_init(void)
{
    Pad_Config(UART_TX_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);
    Pad_Config(UART_RX_PIN, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP, PAD_OUT_DISABLE, PAD_OUT_HIGH);

    Pinmux_Deinit(UART_TX_PIN);
    Pinmux_Deinit(UART_RX_PIN);

    Pinmux_Config(UART_TX_PIN, UART0_TX);
    Pinmux_Config(UART_RX_PIN, UART0_RX);
}

bool driver_uart_init(uint32_t baudrate)
{
    if (! mutex_uart)
        if (! os_mutex_create(&mutex_uart)) return false;

    Rtl_Uart_BaudRate_Config uart_config;
    uart_config = rtl_baud_auto_calc(baudrate);
    if (! uart_config.is_valid) return false;
    
    if (! os_mutex_take(mutex_uart, 0xffffffff))
        return false;

    uart_baudrate_target = baudrate;
    uart_baudrate_actual = uart_config.baud_actual;

    UART_InitTypeDef UART_InitStruct;
    UART_StructInit(&UART_InitStruct);
    
    UART_InitStruct.div            = uart_config.div;
    UART_InitStruct.ovsr           = uart_config.ovsr;
    UART_InitStruct.ovsr_adj       = uart_config.ovsr_adj;

    UART_InitStruct.parity         = UART_PARITY_NO_PARTY;
    UART_InitStruct.stopBits       = UART_STOP_BITS_1;
    UART_InitStruct.wordLen        = UART_WROD_LENGTH_8BIT;
    UART_InitStruct.rxTriggerLevel = 16;                      //1~29
    UART_InitStruct.idle_time      = UART_RX_IDLE_2BYTE;      //idle interrupt wait time

    UART_DeInit(UART0);
    RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);
    UART_Init(UART0, &UART_InitStruct);
    UART_INTConfig(UART, UART_INT_RD_AVA | UART_INT_IDLE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = UART0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelCmd = (FunctionalState)ENABLE;
    NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
    NVIC_Init(&NVIC_InitStruct);

    os_mutex_give(mutex_uart);
    return true;
}

static inline void uart_flush(void)
{
    while (UART_GetFlagState(UART0, UART_FLAG_THR_EMPTY) == 0);
}

bool uart_senddata_continuous(const uint8_t *p_send, uint16_t cnt_send)
{
    if (!p_send || !cnt_send) return false;
    if (!mutex_uart || !os_mutex_take(mutex_uart, 0xffffffff))
        return false;

    while (cnt_send / UART_TX_FIFO_SIZE > 0)
    {
        uart_flush();
        for (uint8_t count = UART_TX_FIFO_SIZE; count > 0; count--)
        {
            UART0->RB_THR = *p_send++;
        }
        cnt_send -= UART_TX_FIFO_SIZE;
    }

    uart_flush();
    while (cnt_send--)
    {
        UART0->RB_THR = *p_send++;
    }
    uart_flush(); //necessary, or else only 1 byte can be sent

    os_mutex_give(mutex_uart);
    return true;
}

void uart_clear_rx_buffer(void)
{
    uart_rx_count = 0;
    flag_rx_data_available = false;

    UART_INTConfig(UART0, UART_INT_RD_AVA | UART_INT_IDLE, ENABLE);
}

static void make_rx_data_available_for_app(void)
{
    if (flag_rx_data_available) return;

    flag_rx_data_available = true;
    T_IO_MSG uart_msg;
    uart_msg.type = IO_MSG_TYPE_UART;
    uart_msg.subtype = IO_MSG_UART_RX;
    send_msg_to_app(&uart_msg);
    APP_PRINT_INFO0("uart.c: make_rx_data_available_for_app()");
}

// interrupt handler
void UART0_Handler(void)
{
    __disable_irq();

    uint32_t int_status = UART_GetIID(UART0);
    UART_INTConfig(UART0, UART_INT_RD_AVA | UART_INT_LINE_STS, DISABLE);

    if (UART_GetFlagState(UART0, UART_FLAG_RX_IDLE) == SET) {
        UART_INTConfig(UART0, UART_INT_IDLE, DISABLE); //clear flag
        make_rx_data_available_for_app();
    }

    if ((int_status & UART_INT_ID_RX_LEVEL_REACH)
    ||  (int_status & UART_INT_ID_RX_TMEOUT))
    {
        uint16_t rx_len = UART_GetRxFIFOLen(UART0),
                 rx_buf_space = Uart_Rx_Buf_Size - uart_rx_count;

        uint16_t rec_len;
        if (rx_len <= rx_buf_space)
            rec_len = rx_len;
        else
            rec_len = rx_buf_space;

        if (rec_len > 0) {
            char* p_buf = uart_rx_buf + uart_rx_count;
            for (uint16_t i = 0; i < rec_len; i++)
                *p_buf++ = (char)UART0->RB_THR;
            uart_rx_count += rec_len;
        }
        if (uart_rx_count == Uart_Rx_Buf_Size) {
            APP_PRINT_INFO0("uart.c: UART0_Handler() buffer is full");
            UART_ClearRxFifo(UART0);
            make_rx_data_available_for_app();
        }
    }

    if (uart_rx_count < Uart_Rx_Buf_Size)
        UART_INTConfig(UART0, UART_INT_RD_AVA, ENABLE);
    __enable_irq();
}
