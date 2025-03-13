# Host Debugger

Simple serial terminal for `rtl8762c-ble-uart`. Building it requires the Rust toolchain, which will consume less disk space than installing Qt build environment if GCC/MSVC(Windows) is already installed.

Currently it requires the device bluetooth MAC address as a parameter, which can be a bit difficult to check on Windows: open Device Manager, double click the paired "RTL-UART-XXXXXX" under "Bluetooth Devices", view "associated endpoint address" property in "Detailed information" tab.

## TODO
- support writing large data blocks through `std::io::Write` trait by splitting data into smaller frames (limited by ATT_MTU - 3);
- check for disabled bluetooth (the adapter is probably still available in `bluest`!).
