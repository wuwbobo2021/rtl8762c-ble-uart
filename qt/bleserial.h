// by wuwbobo2021 <wuwbobo@outlook.com>
// for use with <https://github.com/wuwbobo2021/rtl8762c-ble-uart>
// waitForReadyRead() and waitForBytesWritten() were not implemented

#ifndef BLE_SERIAL_H
#define BLE_SERIAL_H

#include <QIODevice>
#include <QLowEnergyController>
#include <QLowEnergyService>

class BleSerial : public QIODevice
{
    Q_OBJECT
public:
    explicit BleSerial(const QString& remoteDeviceAddr, QObject* parent = nullptr);
    void setBaudrate(quint32 baud);

    bool isSequential() const override {return true;}

    bool isConnected() const {return m_connected;}
    QString deviceName() const;
    qint64 bytesAvailable() const override {
        return QIODevice::bytesAvailable() + m_receivedData.size();
    }

signals:
    void connected();
    void connectionFailed();
    void disconnected();

protected:
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;

private slots:
    void serviceDiscovered(const QBluetoothUuid &);
    void controllerStateChanged(QLowEnergyController::ControllerState state);
    void controllerDisconnected();
    void serviceStateChanged(QLowEnergyService::ServiceState s);
    void characteristicChanged(const QLowEnergyCharacteristic& c, const QByteArray& value);

private:
    QBluetoothUuid m_serviceUuid = QBluetoothUuid((quint16)0xA00A),
                   m_baudUuid    = QBluetoothUuid((quint16)0xB001),
                   m_readUuid    = QBluetoothUuid((quint16)0xB003),
                   m_writeUuid   = QBluetoothUuid((quint16)0xB002);

    QBluetoothAddress m_deviceAddress;
    quint32 m_baudrate = 0;

    QLowEnergyController* m_deviceControl = nullptr;
    QLowEnergyService* m_deviceService = nullptr;
    volatile bool m_connected = false;
    QByteArray m_receivedData;

    void disconnectHandler();
};

#endif
