# rtl8762c-ble-uart
Simple BLE-UART bridge for RTL8762C, with an UART baud rate calculator for RTL8762C and probably RTL8762D, RTL8762E, RTL87x3x, etc (probably the first implementation available on the web).

## Description

This program is based on the ble_peripheral example in RTL8762C-sdk-gcc-v0.0.5.zip, in which I ignored the copyright notices on the basis of `RTL8762C SDK User Guide EN.pdf`: "Generally it is not recommended to create a new project for development, better to open an existing demo project and add developer's own function codes to it." The SDK and related documents are available at the [vendor's site](https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a).

In fact I had been using the RTL8762x Data Transfer Application from the vendor; I made this program because I had to change the UART pins for some reason, but I failed to recompile the datatrans application provided in the GCC SDK package (but not in the SDK package for MDK) and make it work correctly. And it is strange that the version of datatrans source code seems older than that of the latest binary package, and many features look different.

Currently this program differs from the vendor's datatrans application in some ways:
- baud rate is freely configurable via BLE, while the vendor's datatrans supports selecting a value from the table via the UART (AT interface) itself;
- this BLE peripheral can be discovered by Android 8.0 while the vendor's datatrans (v2.1.6.0, 2023-3-22) might not be discovered by Android 8.0 or above versions, that's probably caused by wrong structure length data of the advertisement packet;
- UART Tx/Rx pins can be changed in `board.h` (comparing with the vendor's rock-solid datatrans binary package);
- UART is disabled when DLPS power-saving mode is enabled and BLE is disconnected, and a pin can be selected to output low/high when BLE is connected/disconnected;
- UART AT interface, UART hardware flow control and bluetooth flow control are not available;
- sending data from BLE master to UART Tx is done in the `app` main task, which might be inefficient;
- UART Rx data transfer is not designed for maximum speed: UART receive buffer (4KB) is not implemented as a circular FIFO, and no data is sent before Rx Idle event, thus overflow is more likely to happen when transfering large bulks of data;
- Bluetooth device name is generated from the MAC address and cannot be changed;
- PIN/just-work pairing mode can only be configured in `app_flags.h` and is fixed at runtime;

Why did I choose this chip instead of others with SPP? because it is cheaper and I've got plenty of them.

Based on the vendor's Simple BLE Profile implementation, this application has the same service ID 0xA00A, and its characteristics are listed here:
- 0xB001: baud rate setting, properties: read / write without response. Write the baud rate into it (4B in little-endian), then read it to check the actual baud rate;
- 0xB002: write to the UART Tx, properties: write / write without response;
- 0xB003: receive from UART Rx, properties: notify.

## Compile and flash

Download RTL8762C-sdk-gcc-vx.x.x.zip from the [vendor's site](https://www.realmcu.com/en/Home/Product/93cc0582-3a3f-4ea8-82ea-76c6504e478a) and extract it;

`cd` into `bee2-sdk-gcc-vx.x.x/sdk/tool` and execute:
```
chmod +x memory_icf/MemDefine
chmod +x hex2bin/Hex2Bin
chmod +x prepend_header/prepend_header
chmod +x md5/md5
```

Extract source code files of rtl8762c-ble-uart into a folder, place this folder in `bee2-sdk-gcc-vx.x.x` extracted from the vendor's SDK;

`cd` into this folder, execute `make`, or `bear -- make` for clangd. Make sure that `make` and `gcc-arm-none-eabi` are installed.

Flash it into the chip (address 0x0080E000) via the UART interface, with the Log pin pulled down when it is powered on.

BeeMPTool from the vendor can be used on Windows, otherwise check the [rtltool program](https://github.com/wuwbobo2021/rtltool), which was forked from [cyber-murmel's program](https://github.com/cyber-murmel/rtltool) only to make downloading of BeeMPTool optional.

`ROM_Patch_1.0.611.1.bin` (0x00803000) and `configFile_2023.03.17.15.bin` (0x00801000) were extracted from the vendor's datatrans binary package (512B of checksum is removed for rtltool), flashing them can solve the problem related to DLPS mode. The MAC address `54 00 12 02 E0 00` (`54 00 12` might be the vendor ID) in the configFile can be changed to some other value. If you are using BeeMPTool, just flash the whole datatrans at first.
