// by wuwbobo2021 <wuwbobo@outlook.com>
// for use with <https://github.com/wuwbobo2021/rtl8762c-ble-uart>

use hex::{FromHex, ToHex};
use std::{
    io::{self, Write},
    sync::{Arc, Mutex},
    thread,
    time::Duration,
};

use rtl8762c_ble_uart_host::{BleSerial, BleSerialEvent};

const PROMPT_USAGE: &str = " \
Usage: -u <device_uuid> [-b <baud_rate>] [-h]
\t-h\tHex mode
";

fn main() {
    use tracing_subscriber::prelude::*;
    use tracing_subscriber::{filter::LevelFilter, fmt, EnvFilter};
    tracing_subscriber::registry()
        .with(fmt::layer())
        .with(
            EnvFilter::builder()
                .with_default_directive(LevelFilter::INFO.into())
                .from_env_lossy(),
        )
        .init();

    let (dev_bt_addr, baud_rate, read_timeout_ms, hex_mode, clear_on_disc) = {
        let mut dev_bt_addr: Option<String> = None;
        let mut baud_rate: Option<u32> = None;
        let read_timeout_ms = 500; // timeout value for io::Read, makes no difference here
        let mut hex_mode = false;
        let clear_on_disc = false; // makes no difference in this program

        let mut args = std::env::args();
        let _ = args.next(); //skip program path
        while let Some(s) = args.next() {
            match &s as &str {
                "-u" => dev_bt_addr = Some(args.next().unwrap()),
                "-b" => baud_rate = Some(args.next().unwrap().trim().parse().unwrap()),
                "-h" => hex_mode = true,
                _ => (),
            }
        }
        if dev_bt_addr.is_none() {
            print!("{}", PROMPT_USAGE);
            return;
        }
        (
            dev_bt_addr.unwrap(),
            baud_rate,
            read_timeout_ms,
            hex_mode,
            clear_on_disc,
        )
    };

    let ble_ser = Arc::new(Mutex::new(
        BleSerial::build(&dev_bt_addr, Duration::from_millis(read_timeout_ms)).unwrap(),
    ));

    // clone the Arc smart pointer `ble_ser` for on_event()'s closure
    // without downgrading causes memory leak and forced shutdown on exit
    let ble_ser_weak = Arc::<Mutex<BleSerial>>::downgrade(&ble_ser);
    ble_ser
        .lock()
        .unwrap()
        .on_event(move |evt| {
            let ble_ser = if let Some(s) = ble_ser_weak.upgrade() {
                s
            } else {
                return;
            };
            match evt {
                BleSerialEvent::Connect => {
                    println!("BleSerial Event: Connected");
                    if let Some(baud) = baud_rate {
                        match ble_ser.lock().unwrap().set_baud_rate(baud) {
                            Ok(b) => {
                                println!("BleSerial: Baudrate set. expected: {baud} current: {b}")
                            }
                            Err(opt_b) => println!(
                                "BleSerial: Baudrate not set. expected: {} current: {:?}",
                                baud, opt_b
                            ),
                        }
                    } else {
                        println!(
                            "BleSerial: Baudrate {}",
                            ble_ser.lock().unwrap().baud_rate().unwrap()
                        );
                    }
                }
                BleSerialEvent::Disconnect => {
                    println!("BleSerial Event: Disconnected");
                    if clear_on_disc {
                        let _ = ble_ser.lock().unwrap().drain_read_buf();
                    }
                }
                BleSerialEvent::Receive(data) => {
                    let drain_buf = ble_ser.lock().unwrap().drain_read_buf();
                    assert_eq!(data, drain_buf); //because it's not read elsewhere
                    if hex_mode {
                        println!("BleSerial Receive: {}", &bytes_to_spaced_hex(&data));
                    } else if let Ok(s) = String::from_utf8(data) {
                        println!("BleSerial Receive: {}", s);
                    } else {
                        println!("BleSerial Receive: {}", &bytes_to_spaced_hex(&drain_buf));
                    }
                }
                BleSerialEvent::WriteFailed(data) => {
                    println!(
                        "BleSerial Event: WriteFailed {}",
                        &bytes_to_spaced_hex(&data)
                    );
                }
            }
        })
        .unwrap();

    let mut connected = false;
    let mut cmd_line = String::new();
    println!("enter data to be sent after connected; enter 'blequit' to quit.");
    loop {
        if !connected {
            thread::sleep(Duration::from_millis(50));
            connected = ble_ser.lock().unwrap().is_connected();
            if !connected {
                continue;
            }
        }
        if io::stdin().read_line(&mut cmd_line).is_err() {
            return;
        }
        if cmd_line.trim() == "blequit" {
            return;
        }
        let result = if hex_mode {
            if let Ok(vec_bytes) = Vec::from_hex(cmd_line.replace(" ", "").trim()) {
                ble_ser.lock().unwrap().write(&vec_bytes)
            } else {
                println!("BleSerial: Failed to parse hex input.");
                Err(io::Error::from(io::ErrorKind::InvalidInput))
            }
        } else {
            // TODO: add option for CR, LF, or CR+LF
            ble_ser.lock().unwrap().write(cmd_line.as_bytes())
        };
        if let Err(e) = result {
            if e.kind() != io::ErrorKind::InvalidInput {
                println!("BleSerial: Write failed unexpectedly: {:?}", e);
            }
        }
        cmd_line.clear();
    }
}

// TODO: optimize
fn bytes_to_spaced_hex(bytes: &[u8]) -> String {
    let chars = bytes.encode_hex::<Vec<char>>();
    let mut hex_string = String::new();
    for h in chars.chunks(2) {
        hex_string.push(h[0]);
        hex_string.push(h[1]);
        hex_string.push(' ');
    }
    hex_string
}
