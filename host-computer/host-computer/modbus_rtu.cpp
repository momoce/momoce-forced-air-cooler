#include "modbus_rtu.h"
#include <QSerialPortInfo>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

// ============================================================
// ModbusRTU
// ============================================================
quint16 ModbusRTU::crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= (quint8)data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

QByteArray ModbusRTU::buildFrame(const QByteArray &data)
{
    QByteArray frame = data;
    const quint16 crc = crc16(data);
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));
    return frame;
}

QByteArray ModbusRTU::buildFrame(quint8 addr, quint8 func,
                                 const QByteArray &payload)
{
    QByteArray data;
    data.append(char(addr));
    data.append(char(func));
    data.append(payload);
    return buildFrame(data);
}

bool ModbusRTU::checkFrame(const QByteArray &frame)
{
    if (frame.size() < 4)
        return false;
    const QByteArray data = frame.left(frame.size() - 2);
    const quint16 calcCrc = crc16(data);
    const quint16 recvCrc = (quint8)frame[frame.size() - 2]
                            | ((quint8)frame[frame.size() - 1] << 8);
    return calcCrc == recvCrc;
}

QByteArray ModbusRTU::payloadOf(const QByteArray &frame)
{
    if (frame.size() < 2)
        return QByteArray();
    return frame.left(frame.size() - 2);
}

quint16 ModbusRTU::crcOf(const QByteArray &frame)
{
    if (frame.size() < 2)
        return 0;
    return (quint8)frame[frame.size() - 2]
           | ((quint8)frame[frame.size() - 1] << 8);
}

// ============================================================
// PortProbeWorker
// ============================================================
PortProbeWorker::PortProbeWorker(const QString &portName,
                                 const QByteArray &request,
                                 const QByteArray &expectPrefix,
                                 int expectedLength,
                                 int responseTimeoutMs,
                                 qint32 baudRate)
    : m_portName(portName)
    , m_request(request)
    , m_expectPrefix(expectPrefix)
    , m_expectedLength(expectedLength)
    , m_responseTimeoutMs(responseTimeoutMs)
    , m_baudRate(baudRate)
{
}

void PortProbeWorker::doProbe()
{
    QByteArray response;

    QSerialPort port;
    port.setPortName(m_portName);
    port.setBaudRate(m_baudRate);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);

    // ⭐ 关键：在子线程里 open()，卡住也不影响主线程
    if (!port.open(QIODevice::ReadWrite)) {
        qDebug() << "[Probe]" << m_portName
                 << "打开失败：" << port.errorString();
        emit finished(m_portName, QByteArray());
        return;
    }

    qDebug() << "[Probe]" << m_portName
             << "发送：" << m_request.toHex(' ');

    port.write(m_request);
    port.waitForBytesWritten(50);

    // 轮询读取，最多 responseTimeoutMs
    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < m_responseTimeoutMs) {
        int remain = m_responseTimeoutMs - int(timer.elapsed());
        if (remain <= 0) break;

        int wait = qMin(remain, 20);
        if (port.waitForReadyRead(wait)) {
            buffer.append(port.readAll());
            if (buffer.size() >= m_expectedLength)
                break;
        }
    }

    port.close();

    if (buffer.size() < m_expectedLength) {
        qDebug() << "[Probe]" << m_portName
                 << "响应不足，收到：" << buffer.toHex(' ');
        emit finished(m_portName, QByteArray());
        return;
    }

    response = buffer.left(m_expectedLength);
    qDebug() << "[Probe]" << m_portName
             << "收到：" << response.toHex(' ');

    emit finished(m_portName, response);
}

// ============================================================
// ModbusScanner
// ============================================================
ModbusScanner::ModbusScanner(QObject *parent)
    : QObject(parent)
{
    m_mainTimeout = new QTimer(this);
    m_mainTimeout->setSingleShot(true);
    connect(m_mainTimeout, &QTimer::timeout,
            this, &ModbusScanner::onMainTimeout);
}

ModbusScanner::~ModbusScanner()
{
    cleanupCurrentThread();
}

void ModbusScanner::startScan(const QStringList &portNames,
                              quint8 addr, quint8 func,
                              const QByteArray &payload,
                              const QByteArray &expectPrefix,
                              int expectedLength,
                              int responseTimeoutMs,
                              qint32 baudRate)
{
    if (m_scanning)
        return;

    m_ports = portNames;
    m_foundPorts.clear();
    m_index = -1;

    m_expectedLength    = qMax(4, expectedLength);
    m_responseTimeoutMs = qMax(50, responseTimeoutMs);
    m_baudRate          = baudRate;

    m_request.clear();
    m_request.append(char(addr));
    m_request.append(char(func));
    m_request.append(payload);

    if (expectPrefix.isEmpty()) {
        m_expectPrefix.clear();
        m_expectPrefix.append(char(addr));
        m_expectPrefix.append(char(func));
    } else {
        m_expectPrefix = expectPrefix;
    }

    if (m_ports.isEmpty()) {
        emit scanFinished();
        return;
    }

    m_scanning = true;
    emit scanStarted();
    onNextPort();
}

void ModbusScanner::startScan(quint8 addr, quint8 func,
                              const QByteArray &payload,
                              const QByteArray &expectPrefix,
                              int expectedLength,
                              int responseTimeoutMs,
                              qint32 baudRate)
{
    QStringList all;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports)
        all << info.portName();

    startScan(all, addr, func, payload,
              expectPrefix, expectedLength, responseTimeoutMs, baudRate);
}

void ModbusScanner::stopScan()
{
    if (!m_scanning)
        return;

    m_mainTimeout->stop();
    cleanupCurrentThread();

    m_scanning = false;
    emit scanFinished();
}

void ModbusScanner::onNextPort()
{
    // 清掉上一个线程
    cleanupCurrentThread();

    ++m_index;
    if (m_index >= m_ports.size()) {
        finishScan();
        return;
    }

    const QString portName = m_ports.at(m_index);
    emit portTrying(portName);

    // 创建 worker + 线程
    m_currentThread = new QThread(this);
    m_currentWorker = new PortProbeWorker(portName,
                                          ModbusRTU::buildFrame(m_request),
                                          m_expectPrefix,
                                          m_expectedLength,
                                          m_responseTimeoutMs,
                                          m_baudRate);
    m_currentWorker->moveToThread(m_currentThread);

    connect(m_currentThread, &QThread::started,
            m_currentWorker, &PortProbeWorker::doProbe);
    connect(m_currentWorker, &PortProbeWorker::finished,
            this, &ModbusScanner::onWorkerFinished);
    connect(m_currentWorker, &PortProbeWorker::finished,
            m_currentThread, &QThread::quit);
    connect(m_currentThread, &QThread::finished,
            m_currentWorker, &QObject::deleteLater);
    connect(m_currentThread, &QThread::finished,
            m_currentThread, &QObject::deleteLater);

    m_currentThread->start();

    // 主线程超时：比响应时间稍长一点点（给 IO 一些余量）
    m_mainTimeout->start(m_responseTimeoutMs + 30);
}

void ModbusScanner::onWorkerFinished(const QString &portName,
                                     const QByteArray &response)
{
    m_mainTimeout->stop();

    bool ok = false;
    if (response.size() >= m_expectedLength
        && response.startsWith(m_expectPrefix)
        && ModbusRTU::checkFrame(response.left(m_expectedLength))) {
        ok = true;
    }

    if (ok) {
        qDebug() << "[Scan]" << portName << "发现设备";
        m_foundPorts << portName;
        emit deviceFound(portName);
    } else {
        qDebug() << "[Scan]" << portName << "无效响应";
        emit portTimeout(portName);
    }

    // 断开连接防止重复回调
    if (m_currentWorker)
        disconnect(m_currentWorker, nullptr, this, nullptr);

    QTimer::singleShot(0, this, &ModbusScanner::onNextPort);
}

void ModbusScanner::onMainTimeout()
{
    const QString portName = m_ports.value(m_index);
    qDebug() << "[Scan]" << portName << "主线程超时，放弃";

    // ⭐ 关键：断开 worker 与本对象的连接，之后它自己完成也不会回调到我们
    if (m_currentWorker)
        disconnect(m_currentWorker, nullptr, this, nullptr);

    emit portTimeout(portName);

    // 不等待，直接进入下一个串口
    QTimer::singleShot(0, this, &ModbusScanner::onNextPort);
}

void ModbusScanner::cleanupCurrentThread()
{
    if (!m_currentThread)
        return;

    // 断开与本对象相关的信号
    if (m_currentWorker)
        disconnect(m_currentWorker, nullptr, this, nullptr);

    QThread *thread = m_currentThread;
    m_currentThread = nullptr;
    m_currentWorker = nullptr;

    // 让线程自己退出（worker 在 doProbe 返回后会 emit finished → quit）
    if (thread->isRunning()) {
        thread->quit();
        // 不 wait，避免阻塞主线程；线程对象和 worker 会 deleteLater
        // 如果一定要立刻回收，可以 thread->wait(200)
    }
}

void ModbusScanner::finishScan()
{
    m_scanning = false;
    emit scanFinished();
}