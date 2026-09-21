#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QSerialPort>
#include <QTimer>


class QTimer;


// ============================================================
// ModbusRTU：工具类
// ============================================================
class ModbusRTU
{
public:
    static quint16 crc16(const QByteArray &data);
    static QByteArray buildFrame(const QByteArray &data);
    static QByteArray buildFrame(quint8 addr, quint8 func,
                                 const QByteArray &payload = QByteArray());
    static bool checkFrame(const QByteArray &frame);
    static QByteArray payloadOf(const QByteArray &frame);
    static quint16 crcOf(const QByteArray &frame);
};

// ============================================================
// PortProbeWorker：在子线程里探测一个串口
// ============================================================
class PortProbeWorker : public QObject
{
    Q_OBJECT
public:
    PortProbeWorker(const QString &portName,
                    const QByteArray &request,
                    const QByteArray &expectPrefix,
                    int expectedLength,
                    int responseTimeoutMs,
                    qint32 baudRate);

public slots:
    void doProbe();

signals:
    // response 为空表示失败（打开失败/超时/校验失败）
    void finished(const QString &portName, const QByteArray &response);

private:
    QString     m_portName;
    QByteArray  m_request;
    QByteArray  m_expectPrefix;
    int         m_expectedLength;
    int         m_responseTimeoutMs;
    qint32      m_baudRate;
};

// ============================================================
// ModbusScanner：异步扫描器，每个串口一个线程，主线程超时不等
// ============================================================
class ModbusScanner : public QObject
{
    Q_OBJECT
public:
    explicit ModbusScanner(QObject *parent = nullptr);
    ~ModbusScanner();

    // 扫描指定串口列表
    void startScan(const QStringList &portNames,
                   quint8 addr,
                   quint8 func,
                   const QByteArray &payload = QByteArray(),
                   const QByteArray &expectPrefix = QByteArray(),
                   int expectedLength = 6,
                   int responseTimeoutMs = 200,
                   qint32 baudRate = 9600);

    // 扫描所有可用串口
    void startScan(quint8 addr, quint8 func,
                   const QByteArray &payload = QByteArray(),
                   const QByteArray &expectPrefix = QByteArray(),
                   int expectedLength = 6,
                   int responseTimeoutMs = 200,
                   qint32 baudRate = 9600);

    void stopScan();

    bool isScanning() const { return m_scanning; }
    QStringList foundPorts() const { return m_foundPorts; }

signals:
    void scanStarted();
    void portTrying(const QString &portName);
    void portTimeout(const QString &portName);
    void deviceFound(const QString &portName);
    void scanFinished();

private slots:
    void onNextPort();
    void onWorkerFinished(const QString &portName, const QByteArray &response);
    void onMainTimeout();

private:
    void cleanupCurrentThread();
    void finishScan();

    QStringList  m_ports;
    int          m_index = -1;

    QByteArray   m_request;
    QByteArray   m_expectPrefix;
    int          m_expectedLength = 6;
    int          m_responseTimeoutMs = 200;
    qint32       m_baudRate = 9600;

    QThread     *m_currentThread = nullptr;
    PortProbeWorker *m_currentWorker = nullptr;
    QTimer      *m_mainTimeout = nullptr;

    bool         m_scanning = false;
    QStringList  m_foundPorts;
};

#endif // MODBUS_RTU_H