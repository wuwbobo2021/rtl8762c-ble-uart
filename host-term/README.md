# Host Debugger

Simple serial terminal for `rtl8762c-ble-uart`. Building it requires the Rust toolchain, which will consume less disk space than installing Qt build environment if GCC/MSVC(Windows) is already installed.

## TODO
- support writing large data blocks through `std::io::Write` trait by splitting data into smaller frames (limited by ATT_MTU - 3);
- check for disabled bluetooth (the adapter is probably still available in btleplug!).

## Problems on Windows

Currently it requires the device bluetooth MAC address as a parameter, which can be a bit hard to check on Windows: open Device Manager, double click the paired "RTL-UART-XXXXXX" under "Bluetooth Devices", view "associated endpoint address" property in "Detailed information" tab.

If it failed to connect to the device on Windows: try to make sure the device is already connected (pair again via Settings). Reference: <https://github.com/deviceplug/btleplug/issues/260>, `Error { code: 0x8000FFFF, message: catastrophic failure}`.

### Known issue on older Windows 10 versions (Release < 2004)

Reference: <https://github.com/deviceplug/btleplug/issues/364>, by ChristianPavilonis. It's probably about the Microsoft `windows` crate, which doesn't describe about OS version issues clearly.

To solve the problem before it's fixed by `deviceplug`:

1. download the `btleplug` gzip package from <https://crates.io/api/v1/crates/btleplug/0.11.5/download>, change `.crate` to `.gz` and extract it (for double times) and put folder `btleplug-0.11.5` here;

2. in `btleplug-0.11.5\src\winrtble\ble\watcher.rs`, line 54, change `self.watcher.SetAllowExtendedAdvertisements(true)?;` to `let _ = self.watcher.SetAllowExtendedAdvertisements(true);` and save the file; 

3. in `Cargo.toml`, comment the line `btleplug = "0.11.5"` by `#`, uncomment the line below; run `cargo clean` then `cargo run -r`.

Reference: <https://learn.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.advertisement.bluetoothleadvertisementwatcher.allowextendedadvertisements?view=winrt-26100>
