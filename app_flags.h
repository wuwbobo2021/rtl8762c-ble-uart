// based on the ble_peripheral example in RTL8762C-sdk-gcc-v0.0.5.zip
// <https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a>
// modified by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>

#ifndef _APP_FLAGS_H_
#define _APP_FLAGS_H_

#define APP_MAX_LINKS  1 // value of 2 or above might not work

#define F_BT_DLPS_EN   0 // if enabled, Rx data will not be received when the device is disconnected

#if F_BT_DLPS_EN
    // output low/high on the selected pin when BLE is connected/disconnected
	#define BT_CTRL_SWITCH_EN  0
#endif

// uncomment the line below to enable fixed PIN authentication
//#define AUTHEN_FIXED_PIN 123456 // range: 0 ~ 999,999
#define AUTHEN_RETRY_CNT 3 // never accept any connection once it is reached

#ifdef AUTHEN_FIXED_PIN
    #define SIMP_SRV_AUTHEN_EN 1
#else
    #define SIMP_SRV_AUTHEN_EN 0
#endif

#define FTL_APP_STORE_BAUDRATE_OFFSET  16

/** @} */ /* End of group PERIPH_Config */
#endif
