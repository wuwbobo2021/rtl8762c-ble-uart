// based on the ble_peripheral example in RTL8762C-sdk-gcc-v0.0.5.zip
// <https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a>
// modified by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <os_sched.h>
#include <trace.h>

#include <ftl.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <profile_server.h>
#include <gap_msg.h>

#if F_BT_DLPS_EN
#include <dlps.h>
#include <rtl876x_io_dlps.h>
#endif

#include "simple_ble_service.h"
#include "app_task.h"
#include "peripheral_app.h"
#include "uart.h"


/** @defgroup  PERIPH_DEMO_MAIN Peripheral Main
    * @brief Main file to initialize hardware and BT stack and start task scheduling
    * @{
    */

/*============================================================================*
 *                              Constants
 *============================================================================*/
/** @brief  Default minimum advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define DEFAULT_ADVERTISING_INTERVAL_MIN            320
/** @brief  Default maximum advertising interval */
#define DEFAULT_ADVERTISING_INTERVAL_MAX            320

/*============================================================================*
 *                              Variables
 *============================================================================*/

/** @brief  GAP - scan response data (max size = 31 bytes) */
static const uint8_t scan_rsp_data[] =
{
    0x03,                             /* length */
    GAP_ADTYPE_APPEARANCE,            /* type="Appearance" */
    LO_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
    HI_WORD(GAP_GATT_APPEARANCE_UNKNOWN),
};

/** @brief  GAP - Advertisement data (max size = 31 bytes, best kept short to conserve power) */
static uint8_t adv_data[9 + 15] =
{
    /* Flags */
    0x02,             /* length */
    GAP_ADTYPE_FLAGS, /* type="Flags" */
    GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    /* Service */
    0x03,             /* length */
    GAP_ADTYPE_16BIT_COMPLETE,
    LO_WORD(GATT_UUID_SIMPLE_PROFILE),
    HI_WORD(GATT_UUID_SIMPLE_PROFILE),
    /* Local name */
    0x10,             /* length */
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    /* name string */
};

extern T_SERVER_ID simp_srv_id; //from peripheral_app.c

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
  * @brief  Initialize peripheral and gap bond manager related parameters
  * @return void
  */
void app_le_gap_init(void)
{
    /* Device name and device appearance */
    char device_name[GAP_DEVICE_NAME_LEN] = "RTL-UART-";
    uint8_t local_addr[6];
    gap_get_param(GAP_PARAM_BD_ADDR, local_addr);
    sprintf(device_name + 9, "%02X%02X%02X", local_addr[3], local_addr[4], local_addr[5]);
    memcpy(adv_data + 0x9, device_name, 15);

    uint16_t appearance = GAP_GATT_APPEARANCE_UNKNOWN;
    uint8_t  slave_init_mtu_req = true; //otherwise the MTU will be the minimum size

    /* Advertising parameters */
    uint8_t  adv_evt_type = GAP_ADTYPE_ADV_IND;
    uint8_t  adv_direct_type = GAP_REMOTE_ADDR_LE_PUBLIC;
    uint8_t  adv_direct_addr[GAP_BD_ADDR_LEN] = {0};
    uint8_t  adv_chann_map = GAP_ADVCHAN_ALL;
    uint8_t  adv_filter_policy = GAP_ADV_FILTER_ANY;
    uint16_t adv_int_min = DEFAULT_ADVERTISING_INTERVAL_MIN;
    uint16_t adv_int_max = DEFAULT_ADVERTISING_INTERVAL_MAX;

    /* GAP Bond Manager parameters */
    #if SIMP_SRV_AUTHEN_EN
        uint8_t  auth_pair_mode = GAP_PAIRING_MODE_PAIRABLE;
        uint16_t auth_flags = GAP_AUTHEN_BIT_BONDING_FLAG | GAP_AUTHEN_BIT_MITM_FLAG;
        uint8_t  auth_io_cap = GAP_IO_CAP_DISPLAY_ONLY;
        uint8_t  auth_oob = false;
        uint8_t  auth_use_fix_passkey = true;
        uint32_t auth_fix_passkey = AUTHEN_FIXED_PIN;
        uint8_t  auth_sec_req_enable = true;
        uint16_t auth_sec_req_flags = GAP_AUTHEN_BIT_MITM_FLAG;
    #else
        uint8_t  auth_pair_mode = GAP_PAIRING_MODE_PAIRABLE;
        uint16_t auth_flags = GAP_AUTHEN_BIT_BONDING_FLAG;
        uint8_t  auth_io_cap = GAP_IO_CAP_NO_INPUT_NO_OUTPUT;
        uint8_t  auth_oob = false;
        uint8_t  auth_use_fix_passkey = false;
        uint32_t auth_fix_passkey = 0;
        uint8_t  auth_sec_req_enable = false;
        uint16_t auth_sec_req_flags = GAP_AUTHEN_BIT_BONDING_FLAG;
    #endif

    /* Set device name and device appearance */
    le_set_gap_param(GAP_PARAM_DEVICE_NAME, GAP_DEVICE_NAME_LEN, device_name);
    le_set_gap_param(GAP_PARAM_APPEARANCE, sizeof(appearance), &appearance);
    le_set_gap_param(GAP_PARAM_SLAVE_INIT_GATT_MTU_REQ, sizeof(slave_init_mtu_req),
                     &slave_init_mtu_req);

    /* Set advertising parameters */
    le_adv_set_param(GAP_PARAM_ADV_EVENT_TYPE, sizeof(adv_evt_type), &adv_evt_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, sizeof(adv_direct_type), &adv_direct_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR, sizeof(adv_direct_addr), adv_direct_addr);
    le_adv_set_param(GAP_PARAM_ADV_CHANNEL_MAP, sizeof(adv_chann_map), &adv_chann_map);
    le_adv_set_param(GAP_PARAM_ADV_FILTER_POLICY, sizeof(adv_filter_policy), &adv_filter_policy);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
    le_adv_set_param(GAP_PARAM_ADV_DATA, sizeof(adv_data), (void *)adv_data);
    le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(scan_rsp_data), (void *)scan_rsp_data);

    /* Setup the GAP Bond Manager */
    gap_set_param(GAP_PARAM_BOND_PAIRING_MODE, sizeof(auth_pair_mode), &auth_pair_mode);
    gap_set_param(GAP_PARAM_BOND_AUTHEN_REQUIREMENTS_FLAGS, sizeof(auth_flags), &auth_flags);
    gap_set_param(GAP_PARAM_BOND_IO_CAPABILITIES, sizeof(auth_io_cap), &auth_io_cap);
    gap_set_param(GAP_PARAM_BOND_OOB_ENABLED, sizeof(auth_oob), &auth_oob);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY, sizeof(auth_fix_passkey), &auth_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_FIXED_PASSKEY_ENABLE, sizeof(auth_use_fix_passkey),
                      &auth_use_fix_passkey);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_ENABLE, sizeof(auth_sec_req_enable), &auth_sec_req_enable);
    le_bond_set_param(GAP_PARAM_BOND_SEC_REQ_REQUIREMENT, sizeof(auth_sec_req_flags),
                      &auth_sec_req_flags);

    /* register gap message callback */
    le_register_app_cb(app_gap_callback);
}

/**
 * @brief  Add GATT services and register callbacks
 * @return void
 */
void app_le_profile_init(void)
{
    server_init(2);
    simp_srv_id = simp_ble_service_add_service(app_profile_callback);
    server_register_app_cb(app_profile_callback);
}

/**
 * @brief    Contains the initialization of pinmux settings and pad settings
 * @note     All the pinmux settings and pad settings shall be initiated in this function,
 *           but if legacy driver is used, the initialization of pinmux setting and pad setting
 *           should be peformed with the IO initializing.
 * @return   void
 */
void board_init(void)
{
    board_uart_init();
}

/**
 * @brief    Contains the initialization of peripherals
 * @note     Both new architecture driver and legacy driver initialization method can be used
 * @return   void
 */
void driver_init(void)
{
    static uint32_t baudrate = 9600;
    if (ftl_load(&baudrate, FTL_APP_STORE_BAUDRATE_OFFSET, 4) != 0) {
        baudrate = 9600;
        ftl_save(&baudrate, FTL_APP_STORE_BAUDRATE_OFFSET, 4);
    }
    driver_uart_init(baudrate);

    le_adv_set_tx_power(GAP_ADV_TX_POW_SET_1M, 0xD0); //7.5 dBm
}

/**
 * @brief    Contains the power mode settings
 * @return   void
 */
void io_dlps_enter_cb(void)
{

}

void io_dlps_exit_cb(void)
{

}

void pwr_mgr_init(void)
{
#if F_BT_DLPS_EN
    DLPS_IORegUserDlpsEnterCb(io_dlps_enter_cb);
    DLPS_IORegUserDlpsExitCb(io_dlps_exit_cb);
    DLPS_IORegister();
    lps_mode_set(LPM_DLPS_MODE);
#endif
}

/**
 * @brief    Contains the initialization of all tasks
 * @note     There is only one task in BLE Peripheral APP, thus only one APP task is init here
 * @return   void
 */
void task_init(void)
{
    app_task_init();
}

/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int main(void)
{
    extern uint32_t random_seed_value;
    srand(random_seed_value);
    board_init();
    le_gap_init(APP_MAX_LINKS);
    gap_lib_init();
    app_le_gap_init();
    app_le_profile_init();
    pwr_mgr_init();
    task_init();
    os_sched_start();

    return 0;
}
/** @} */ /* End of group PERIPH_DEMO_MAIN */


