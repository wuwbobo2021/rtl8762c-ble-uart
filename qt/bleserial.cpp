// by wuwbobo2021 <wuwbobo@outlook.com>
// for use with <https://github.com/wuwbobo2021/rtl8762c-ble-uart>

#include "bleserial.h"
#include <QtEndian>
#include <QTimer>
#include <QBluetoothDeviceInfo>

BleSerial::BleSerial(const QString& remoteDeviceAddr, QObject* parent)
        : m_deviceAddress(remoteDeviceAddr), QIODevice{parent}
{
    setOpenMode(QIODevice::ReadWrite);

    QBluetoothDeviceInfo dev_info(m_deviceAddress, this->deviceName(), 0);
    m_deviceControl = QLowEnergyController::createCentral(dev_info, this);
    m_deviceControl->setRemoteAddressType(QLowEnergyController::PublicAddress);

    connect(m_deviceControl, &QLowEnergyController::connected,
            m_deviceControl, &QLowEnergyController::discoverServices);

    connect(m_deviceControl, &QLowEnergyController::serviceDiscovered,
            this, &BleSerial::serviceDiscovered);
    connect(m_deviceControl, &QLowEnergyController::stateChanged,
            this, &BleSerial::controllerStateChanged);
    connect(m_deviceControl, &QLowEnergyController::disconnected,
            this, &BleSerial::controllerDisconnected);

    m_deviceControl->connectToDevice();
}

void BleSerial::serviceDiscovered(const QBluetoothUuid& uuid)
{
    if (uuid != m_serviceUuid) {
        qDebug("BleSerial::serviceDiscovered(): wrong service uuid.");
        return;
    }
    qDebug("BleSerial::serviceDiscovered(): correct service uuid.");
    m_deviceService = m_deviceControl->createServiceObject(m_serviceUuid, this);
    connect(m_deviceService, &QLowEnergyService::stateChanged,
            this, &BleSerial::serviceStateChanged);
    connect(m_deviceService, &QLowEnergyService::characteristicChanged,
            this, &BleSerial::characteristicChanged);
    m_deviceService->discoverDetails();
    qDebug("BleSerial::serviceDiscovered(): discoverDetails() called.");
}

void BleSerial::controllerStateChanged(QLowEnergyController::ControllerState state)
{
    if (state == QLowEnergyController::DiscoveredState) {
        if (! m_deviceControl->services().contains(m_serviceUuid)) {
            qDebug("BleSerial::controllerStateChanged(): service not found.");
            this->disconnectHandler();
        }
    }
    else if (state == QLowEnergyController::ClosingState
    ||  state == QLowEnergyController::UnconnectedState) {
        // also emitted if the device cannot be connected
        if (m_deviceService)
            qDebug("BleSerial::controllerStateChanged(): disconnected.");
        this->disconnectHandler();
    }
}

void BleSerial::controllerDisconnected()
{
    if (! m_deviceService) return;
    qDebug("BleSerial::controllerDisconnected(): disconnected.");
    this->disconnectHandler();
}

void BleSerial::serviceStateChanged(QLowEnergyService::ServiceState s)
{
    if (s == QLowEnergyService::ServiceDiscovered) {
        const QLowEnergyCharacteristic ch_baud  = m_deviceService->characteristic(m_baudUuid),
                                       ch_read  = m_deviceService->characteristic(m_readUuid),
                                       ch_write = m_deviceService->characteristic(m_writeUuid);
        if (!ch_baud.isValid() || !ch_read.isValid() || !ch_write.isValid()) {
            qDebug("BleSerial::serviceStateChanged(): wrong characteristics in the service.");
            BleSerial::disconnectHandler(); return;
        }

        this->setBaudrate(m_baudrate);

        QLowEnergyDescriptor desc_read = ch_read.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
        if (! desc_read.isValid()) {
            qDebug("BleSerial::serviceStateChanged(): failed to configure read characteristic.");
            BleSerial::disconnectHandler(); return;
        }
        qDebug("BleSerial::serviceStateChanged(): characteristic found, connected.");
        m_deviceService->writeDescriptor(desc_read, QByteArray::fromHex("0100")); //enable notification
        m_connected = true; emit this->connected();
    }
    else if (s == QLowEnergyService::InvalidService) {
        qDebug("BleSerial::serviceStateChanged(): disconnected.");
        this->disconnectHandler();
    }
}

void BleSerial::characteristicChanged(const QLowEnergyCharacteristic& c, const QByteArray& value)
{
    if (c.uuid() != m_readUuid) return;
    this->m_receivedData.append(value);
    qDebug("BleSerial::characteristicChanged(): value readed into buffer.");
    emit readyRead();
}

void BleSerial::disconnectHandler()
{
    qDebug("BleSerial::disconnectHandler(): being called.");

    bool srv_obj_created = (m_deviceService != nullptr);
    if (srv_obj_created) {
        m_deviceService->deleteLater(); m_deviceService = nullptr;
    }

    if (m_connected) {
        m_connected = false;
        emit this->disconnected();
    } else if (srv_obj_created)
        emit this->connectionFailed();

    QTimer::singleShot(5000, m_deviceControl, &QLowEnergyController::connectToDevice);

    qDebug("BleSerial::disconnectHandler(): completed.");
}

QString BleSerial::deviceName() const
{
    quint64 addr_int = m_deviceAddress.toUInt64() >> 24;
    char bytes[3] = {(char)(addr_int & 0xff),
                     (char)((addr_int >> 8) & 0xff),
                     (char)((addr_int >> 16) & 0xff)};
    QByteArray addr_arr(bytes, 3);
    return "RTL-UART-" + addr_arr.toHex(0).toUpper();
}

void BleSerial::setBaudrate(quint32 baud)
{
    if (! baud) return;
    m_baudrate = baud;
    if (! m_deviceService) return;

    quint32_le baud_le(baud); //little endian
    const char* data = (const char*)&baud_le;
    const QLowEnergyCharacteristic ch = m_deviceService->characteristic(m_baudUuid);
    m_deviceService->writeCharacteristic(ch, QByteArray(data, 4),
                                         QLowEnergyService::WriteWithoutResponse);
}

// ---------------------------- QIODevice ----------------------------

qint64 BleSerial::readData(char* data, qint64 max_len)
{
    int sz = std::min(m_receivedData.size(), int(max_len));
    if (sz <= 0) return sz;
    memcpy(data, m_receivedData.constData(), size_t(sz));
    m_receivedData.remove(0, sz);
    qDebug("BleSerial::readData(): data taken from receive buffer.");
    return sz;
}

qint64 BleSerial::writeData(const char* data, qint64 len)
{
    if (!data || !len) return 0;
    if (! m_deviceService) return -1;
    const QLowEnergyCharacteristic ch = m_deviceService->characteristic(m_writeUuid);
    if (! ch.isValid()) return -1;
    m_deviceService->writeCharacteristic(ch, QByteArray(data, int(len)),
                                         QLowEnergyService::WriteWithoutResponse);
    qDebug("BleSerial::writeData(): success.");
    return true;
}
