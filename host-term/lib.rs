// by wuwbobo2021 <wuwbobo@outlook.com>
// for use with <https://github.com/wuwbobo2021/rtl8762c-ble-uart>

const UUID_SERV: Uuid = bluetooth_uuid_from_u16(0xA00A);

const UUID_CHAR_BAUD: Uuid = bluetooth_uuid_from_u16(0xB001);
const UUID_CHAR_READ: Uuid = bluetooth_uuid_from_u16(0xB003);
const UUID_CHAR_WRITE: Uuid = bluetooth_uuid_from_u16(0xB002);

const UUID_DESC_CLIENT_CHAR_CONF: Uuid = bluetooth_uuid_from_u16(0x2902);

#[cfg(feature = "debug")]
macro_rules! debug {
    ($($arg:tt)+) => (eprintln!($($arg)+))
}

#[cfg(not(feature = "debug"))]
macro_rules! debug {
    ($($arg:tt)+) => {};
}

use std::{
    collections::VecDeque,
    io::{self, Read, Write},
    pin::Pin,
    sync::{Arc, Mutex},
    thread,
    time::{Duration, SystemTime},
};

use bluest::{btuuid::bluetooth_uuid_from_u16, Characteristic};

use futures::StreamExt;
use uuid::Uuid;

pub enum BleSerialEvent {
    Connect,
    Disconnect,
    Receive(Vec<u8>),
    WriteFailed(Vec<u8>),
}

enum BleHdlMsg {
    ReqSetBaud(u32),
    ReqWrite(Vec<u8>),
    ReqDrop,
    ReadNotify(Vec<u8>),
    Timer,
}
type PinnedMsgStream = Pin<Box<dyn tokio_stream::Stream<Item = BleHdlMsg> + Send>>;

struct BleSerialRes {
    rt: Option<tokio::runtime::Runtime>,
    dev_addr: String, //cannot be changed
    dev_name: Option<String>,
    baud_rate: u32,
    buf_read: VecDeque<u8>,
    ch_req: Option<tokio::sync::mpsc::UnboundedSender<BleHdlMsg>>,
    on_event: Arc<Box<dyn Fn(BleSerialEvent) + 'static + Send + Sync>>,
}

pub struct BleSerial {
    res: Arc<Mutex<BleSerialRes>>,
    read_timeout: Duration,
}

impl BleSerial {
    pub fn build(device_bt_addr: &str, read_timeout: Duration) -> Result<Self, &'static str> {
        // the default Runtime::new() will create a thread for each CPU core (too many threads)
        let rt = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(2)
            .enable_all()
            .build()
            .map_err(|_| "can't create async runtime required by the bluetooth library")?;

        let res = BleSerialRes {
            rt: Some(rt),
            dev_addr: device_bt_addr.to_string(),
            dev_name: None,
            baud_rate: 9600_u32,
            buf_read: VecDeque::<u8>::new(),
            ch_req: None,
            on_event: Arc::new(Box::new(|_| {})),
        };
        let arc_res = Arc::new(Mutex::new(res));
        let arc_res_2 = arc_res.clone();

        arc_res
            .lock()
            .map_err(|_| "unexpected error")?
            .rt
            .as_ref()
            .unwrap()
            .spawn(Self::ble_loop(arc_res_2));
        Ok(Self {
            res: arc_res,
            read_timeout,
        })
    }

    pub fn is_connected(&self) -> bool {
        self.device_name().is_some()
    }

    pub fn device_name(&self) -> Option<String> {
        if let Ok(lck_res) = self.res.lock() {
            lck_res.dev_name.clone()
        } else {
            None
        }
    }

    pub fn baud_rate(&self) -> Option<u32> {
        if self.is_connected() {
            Some(self.res.lock().unwrap().baud_rate)
        } else {
            None
        }
    }

    pub fn set_baud_rate(&self, baud: u32) -> Result<u32, Option<u32>> {
        if baud == 0 {
            return Err(self.baud_rate());
        }

        let lck_res = self.res.lock().map_err(|_| None)?;
        if lck_res.ch_req.is_none() || lck_res.dev_name.is_none() {
            return Err(None);
        }
        lck_res
            .ch_req
            .as_ref()
            .unwrap()
            .send(BleHdlMsg::ReqSetBaud(baud))
            .map_err(|_| lck_res.baud_rate)?;
        drop(lck_res);

        for _ in 0..10 {
            thread::sleep(Duration::from_millis(1000));
            let cur_baud = self.res.lock().map_err(|_| None)?.baud_rate;
            if Self::baud_acceptable(cur_baud, baud) {
                return Ok(cur_baud);
            }
        }
        Err(self.baud_rate())
    }

    pub fn drain_read_buf(&self) -> Vec<u8> {
        if let Ok(mut lck_res) = self.res.lock() {
            lck_res.buf_read.drain(..).collect::<Vec<u8>>()
        } else {
            Vec::new()
        }
    }

    pub fn on_event(
        &self,
        f: impl Fn(BleSerialEvent) + 'static + Send + Sync,
    ) -> Result<(), &'static str> {
        let mut lck_res = self
            .res
            .lock()
            .map_err(|_| "error that shouldn't happen: unable to set event handler")?;
        lck_res.on_event = Arc::new(Box::new(f));
        Ok(())
    }

    async fn ble_loop(res: Arc<Mutex<BleSerialRes>>) {
        debug!("ble_loop(): entered.");

        let dev_addr = res.lock().as_ref().unwrap().dev_addr.clone();

        // TODO: deal with disabled bluetooth adapter
        let Some(adapter) = bluest::Adapter::default().await else {
            debug!("ble_loop(): bluetooth adapter not found.");
            return;
        };
        adapter.wait_available().await.unwrap();

        loop {
            // create `req` (external call) message channel as soon as possible
            let (tx_req, mut rx_req) = tokio::sync::mpsc::unbounded_channel::<BleHdlMsg>();
            res.lock().as_mut().unwrap().ch_req.replace(tx_req);
            let mut msg_map = tokio_stream::StreamMap::new();
            msg_map.insert(
                "req",
                tokio_stream::StreamNotifyClose::new(Box::pin(async_stream::stream! {
                    while let Some(item) = rx_req.recv().await {
                        yield item;
                    }
                }) as PinnedMsgStream),
            );

            // indicate disconnection
            let prev_name = res.lock().unwrap().dev_name.take();
            if prev_name.is_some() {
                debug!("ble_loop(): disconnected.");
                Self::raise_event(&res, BleSerialEvent::Disconnect);
            }

            // avoid useless retrying if the bluetooth device is not present
            tokio::time::sleep(Duration::from_millis(1500)).await;

            let filter = [UUID_SERV];
            let Ok(mut discoverer) = adapter.discover_devices(&filter).await else {
                debug!("ble_loop(): discover_devices failed.");
                continue;
            };
            debug!("ble_loop(): started discovering.");

            // check the device's MAC address
            let mut device = None;
            'outer: for _ in 0..10 {
                tokio::time::sleep(Duration::from_millis(1000)).await;
                while let Some(Ok(dev)) = discoverer.next().await {
                    let id = dev.id().to_string();
                    println!("ble_loop(): found {}.", &id);
                    if id.to_lowercase().contains(&dev_addr.to_lowercase()) {
                        device = Some(dev);
                        break 'outer;
                    }
                }
            }
            let Some(device) = device else {
                debug!("ble_loop(): target device not found.");
                continue;
            };
            drop(discoverer);

            // connect and get the characteristics
            if adapter.connect_device(&device).await.is_err() {
                debug!("ble_loop(): failed to connect.");
                continue;
            }
            if device.discover_services().await.is_err() {
                debug!("ble_loop(): failed to discover services (unexpected).");
                continue;
            }
            let Ok(services) = device.services().await else {
                debug!("ble_loop(): cannot get device services (unexpected).");
                continue;
            };
            let Some(service) = services.iter().find(|serv| serv.uuid() == UUID_SERV) else {
                debug!("ble_loop(): cannot find the correct service (unexpected).");
                continue;
            };
            let Ok(chars) = service.characteristics().await else {
                debug!("ble_loop(): cannot get service characteristics (unexpected).");
                continue;
            };
            let (char_baud, char_read, char_write) = {
                let (mut ch_baud, mut ch_read, mut ch_write) = (None, None, None);
                for ch in chars {
                    match ch.uuid() {
                        UUID_CHAR_BAUD => ch_baud.replace(ch),
                        UUID_CHAR_READ => ch_read.replace(ch),
                        UUID_CHAR_WRITE => ch_write.replace(ch),
                        _ => None,
                    };
                }
                if ch_baud.is_none() || ch_read.is_none() || ch_write.is_none() {
                    debug!("ble_loop(): incorrect characteristics.");
                    continue;
                }
                (ch_baud.unwrap(), ch_read.unwrap(), ch_write.unwrap())
            };

            if let Some(baud) = Self::read_baud(&char_baud).await {
                res.lock().unwrap().baud_rate = baud;
            } else {
                debug!("ble_loop(): failed to check baud rate.");
                continue;
            }

            // enable read notification
            let Some(desc_char_conf) = char_read.descriptors().await.ok().and_then(|descs| {
                descs
                    .into_iter()
                    .find(|d| d.uuid() == UUID_DESC_CLIENT_CHAR_CONF)
            }) else {
                debug!("ble_loop(): failed to get conf desc of char_read.");
                continue;
            };
            if desc_char_conf.write(&[0x01, 0x00]).await.is_err() {
                // enable notification
                debug!("ble_loop(): failed to write conf desc of char_read.");
            }

            // create UART read notification stream

            msg_map.insert(
                "read",
                tokio_stream::StreamNotifyClose::new(Box::pin(async_stream::stream! {
                    use futures::stream::StreamExt;
                    // XXX: remove this `unwrap()`.
                    let mut stream_notify_read = char_read.notify().await.unwrap();
                    while let Some(Ok(item)) = stream_notify_read.next().await {
                        yield BleHdlMsg::ReadNotify(item);
                    }
                }) as PinnedMsgStream),
            );
            msg_map.insert(
                "timer",
                tokio_stream::StreamNotifyClose::new(Box::pin(async_stream::stream! {
                    loop {
                        tokio::time::sleep(Duration::from_millis(2000)).await;
                        yield BleHdlMsg::Timer;
                    }
                }) as PinnedMsgStream),
            );

            // get device name and indicate for connection
            let dev_name = device.name_async().await.unwrap_or("unknown".to_string());
            res.lock().unwrap().dev_name.replace(dev_name);
            Self::raise_event(&res, BleSerialEvent::Connect);

            // handle messages
            while let Some((key, msg)) = msg_map.next().await {
                if msg.is_none() {
                    debug!("ble_loop(): stream {key} ends, breaking.");
                    break; // the BLE connection is broken, or the req stream is broken
                }
                let msg = msg.unwrap();
                if key == "read" {
                    // read notification
                    if let BleHdlMsg::ReadNotify(data) = msg {
                        // With `VecDeque`, all data should be written into it
                        let _ = res.lock().as_mut().unwrap().buf_read.write(&data).unwrap();
                        Self::raise_event(&res, BleSerialEvent::Receive(data));
                    }
                    continue;
                } else if key == "timer" {
                    // connection checker
                    // TODO: check for disabled bluetooth (adapter is probably still available in bluest!)
                    if !device.is_connected().await {
                        debug!("ble_loop(): disconnected, breaking.");
                        break;
                    }
                    continue;
                }
                match msg {
                    // request message
                    BleHdlMsg::ReqSetBaud(baud) => {
                        for _ in 0..3 {
                            if char_baud
                                .write_without_response(&baud.to_le_bytes())
                                .await
                                .is_ok()
                            {
                                break;
                            }
                        }
                        let mut suc = false;
                        for _ in 0..10 {
                            let cur_baud = Self::read_baud(&char_baud).await.unwrap_or(0);
                            if Self::baud_acceptable(cur_baud, baud) {
                                debug!("ble_loop(): baudrate set.");
                                res.lock().unwrap().baud_rate = cur_baud;
                                suc = true;
                                break;
                            } else {
                                tokio::time::sleep(Duration::from_millis(400)).await;
                            }
                        }
                        if !suc {
                            debug!("ble_loop(): failed to set baud rate.");
                        }
                    }
                    BleHdlMsg::ReqWrite(data) => {
                        // TODO: handle larger data block to be sent
                        let mut suc = false;
                        for _ in 0..3 {
                            if char_write.write(&data).await.is_ok() {
                                suc = true;
                                break;
                            }
                        }
                        if !suc {
                            debug!("ble_loop(): write failed.");
                            Self::raise_event(&res, BleSerialEvent::WriteFailed(data));
                        }
                    }
                    BleHdlMsg::ReqDrop => {
                        debug!("ble_loop(): ready to be dropped, return.");
                        return;
                    }
                    _ => (),
                }
            }
        }
    }

    async fn read_baud(char_baud: &Characteristic) -> Option<u32> {
        for _ in 0..3 {
            if let Ok(bytes_baud) = char_baud.read().await {
                if bytes_baud.len() < 4 {
                    continue;
                }
                let mut arr_baud = [0u8; 4];
                arr_baud.copy_from_slice(&bytes_baud[..4]);
                return Some(u32::from_le_bytes(arr_baud));
            } else {
                continue;
            }
        }
        None
    }

    fn raise_event(res: &Arc<Mutex<BleSerialRes>>, evt: BleSerialEvent) {
        let mut lck_res = res.lock().unwrap();
        let on_event = lck_res.on_event.clone();
        let rt = lck_res.rt.take();
        if rt.is_none() {
            return;
        }
        drop(lck_res);

        rt.as_ref().unwrap().spawn_blocking(move || on_event(evt));
        res.lock().unwrap().rt.replace(rt.unwrap());
    }

    fn baud_acceptable(baud: u32, baud_expected: u32) -> bool {
        if baud == 0 || baud_expected == 0 {
            return false;
        }
        let t_baud = 1. / (baud as f64);
        let t_baud_exp = 1. / (baud_expected as f64);
        f64::abs(t_baud - t_baud_exp) / t_baud_exp <= 0.05
    }
}

impl Read for BleSerial {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if buf.is_empty() {
            return Ok(0);
        }

        let t_timeout = SystemTime::now() + self.read_timeout;
        let mut cnt_read = 0;
        while cnt_read < buf.len() {
            let mut lck_res = self
                .res
                .lock()
                .map_err(|_| io::Error::from(io::ErrorKind::Other))?;
            if let Ok(cnt) = lck_res.buf_read.read(&mut buf[cnt_read..]) {
                cnt_read += cnt;
            }
            drop(lck_res);
            if SystemTime::now() < t_timeout {
                thread::sleep(Duration::from_millis(30));
            } else {
                break;
            }
        }

        if cnt_read == 0 {
            Err(io::Error::from(io::ErrorKind::TimedOut))
        } else {
            Ok(cnt_read)
        }
    }
}

impl Write for BleSerial {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if buf.is_empty() {
            return Ok(0);
        }

        let lck_res = self
            .res
            .lock()
            .map_err(|_| io::Error::from(io::ErrorKind::Other))?;

        if lck_res.ch_req.is_none() || lck_res.dev_name.is_none() {
            return Err(io::Error::from(io::ErrorKind::NotConnected));
        }
        lck_res
            .ch_req
            .as_ref()
            .unwrap()
            .send(BleHdlMsg::ReqWrite(buf.to_vec()))
            .map_err(io::Error::other)?;
        Ok(buf.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl Drop for BleSerial {
    fn drop(&mut self) {
        debug!("BleSerial::drop(): entered.");
        if let Ok(mut lck_res) = self.res.lock() {
            if let Some(ch_req) = lck_res.ch_req.take() {
                if ch_req.send(BleHdlMsg::ReqDrop).is_ok() {
                    let rt = lck_res.rt.take().unwrap();
                    drop(lck_res);
                    rt.shutdown_timeout(Duration::from_millis(2000));
                    debug!("BleSerial::drop(): shutdown_timeout() called.");
                }
            }
        }
    }
}
