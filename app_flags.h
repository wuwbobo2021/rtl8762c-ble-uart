// based on the ble_peripheral example in RTL8762C-sdk-gcc-v0.0.5.zip
// <https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a>
// modified by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

#ifndef _APP_FLAGS_H_
#define _APP_FLAGS_H_


/** @defgroup  PERIPH_Config Peripheral App Configuration
    * @brief This file is used to config app functions.
    * @{
    */
/*============================================================================*
 *                              Constants
 *============================================================================*/

/** @brief  Config APP LE link number */
#define APP_MAX_LINKS  1
/** @brief  Config DLPS: 0-Disable DLPS, 1-Enable DLPS */
#define F_BT_DLPS_EN  0 //currently doesn't work

// PIN range: 0 ~ 999,999
//#define AUTHEN_FIXED_PIN 123456

#ifdef AUTHEN_FIXED_PIN
    #define SIMP_SRV_AUTHEN_EN 1
#else
    #define SIMP_SRV_AUTHEN_EN 0
#endif

#define FTL_APP_STORE_BAUDRATE_OFFSET  16

/** @} */ /* End of group PERIPH_Config */
#endif
