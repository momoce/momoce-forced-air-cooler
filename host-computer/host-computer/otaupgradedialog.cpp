#include "otaupgradedialog.h"
#include "modbus_rtu.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QSerialPort>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QThread>

// ============================================================
// Modbus CRC16
// ============================================================
static quint16 modbusCrc16(const quint8 *data, int len)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

OtaUpgradeDialog::OtaUpgradeDialog(QSerialPort *serialPort, QWidget *parent)
    : QDialog(parent)
    , m_serialPort(serialPort)
{
    setupUI();
}

OtaUpgradeDialog::~OtaUpgradeDialog()
{
    closeTransport();
}

// ============================================================
// 界面
// ============================================================
void OtaUpgradeDialog::setupUI()
{
    setWindowTitle(QStringLiteral("OTA 固件升级"));
    resize(580, 560);

    setStyleSheet(
        "QDialog { background: #FAFAFA; }"
        "QLabel { color: #333; font-size: 13px; }"
        "QLineEdit {"
        "    border: 1px solid #CCC; border-radius: 4px;"
        "    padding: 4px 8px; font-size: 13px;"
        "}"
        "QSpinBox {"
        "    border: 1px solid #CCC; border-radius: 4px;"
        "    padding: 4px 8px; font-size: 13px;"
        "}"
        "QPushButton {"
        "    background: #4CAF50; color: white;"
        "    border: none; border-radius: 6px;"
        "    padding: 6px 16px; font-size: 13px;"
        "}"
        "QPushButton:disabled { background: #BDBDBD; }"
        "QPushButton#secondary { background: #9E9E9E; }"
        "QRadioButton { font-size: 13px; }"
        "QProgressBar {"
        "    border: 1px solid #CCC; border-radius: 6px;"
        "    text-align: center; background: white; height: 20px;"
        "}"
        "QProgressBar::chunk { background: #4CAF50; border-radius: 6px; }"
        "QPlainTextEdit {"
        "    background: #1E1E1E; color: #D4D4D4;"
        "    border-radius: 6px; font-family: Consolas, monospace;"
        "    font-size: 12px;"
        "}"
        );

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // ---- 升级方式 ----
    auto *modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(QStringLiteral("升级方式：")));

    m_rdoWired = new QRadioButton(QStringLiteral("有线串口"));
    m_rdoWifi  = new QRadioButton(QStringLiteral("WiFi"));
    m_rdoWired->setChecked(true);

    auto *modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_rdoWired);
    modeGroup->addButton(m_rdoWifi);

    modeLayout->addWidget(m_rdoWired);
    modeLayout->addWidget(m_rdoWifi);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    // ---- WiFi 参数 ----
    auto *wifiLayout = new QGridLayout();
    wifiLayout->setHorizontalSpacing(10);
    wifiLayout->setVerticalSpacing(6);

    m_lblWifiIp    = new QLabel(QStringLiteral("IP:"));
    m_editWifiIp   = new QLineEdit(QStringLiteral("192.168.4.1"));
    m_lblWifiPort  = new QLabel(QStringLiteral("端口:"));
    m_editWifiPort = new QLineEdit(QStringLiteral("5000"));

    wifiLayout->addWidget(m_lblWifiIp,    0, 0);
    wifiLayout->addWidget(m_editWifiIp,   0, 1);
    wifiLayout->addWidget(m_lblWifiPort,  0, 2);
    wifiLayout->addWidget(m_editWifiPort, 0, 3);

    mainLayout->addLayout(wifiLayout);

    auto updateWifiUi = [this]() {
        bool wifi = m_rdoWifi->isChecked();
        m_lblWifiIp->setVisible(wifi);
        m_editWifiIp->setVisible(wifi);
        m_lblWifiPort->setVisible(wifi);
        m_editWifiPort->setVisible(wifi);
    };
    connect(m_rdoWired, &QRadioButton::toggled, this, updateWifiUi);
    connect(m_rdoWifi,  &QRadioButton::toggled, this, updateWifiUi);
    updateWifiUi();

    // ---- 文件选择区 ----
    auto *fileLayout = new QGridLayout();
    fileLayout->setHorizontalSpacing(10);
    fileLayout->setVerticalSpacing(6);

    m_btnSelect = new QPushButton(QStringLiteral("选择 bin 文件"));
    connect(m_btnSelect, &QPushButton::clicked,
            this, &OtaUpgradeDialog::onSelectFile);

    m_lblFile = new QLabel(QStringLiteral("未选择文件"));
    m_lblFile->setWordWrap(true);
    m_lblSize = new QLabel(QStringLiteral("大小: -"));
    m_lblCrc  = new QLabel(QStringLiteral("CRC16: -"));

    fileLayout->addWidget(m_btnSelect, 0, 0);
    fileLayout->addWidget(m_lblFile,   0, 1);
    fileLayout->addWidget(m_lblSize,   1, 1);
    fileLayout->addWidget(m_lblCrc,    2, 1);

    mainLayout->addLayout(fileLayout);

    // ---- 传输参数 ----
    auto *paramLayout = new QGridLayout();
    paramLayout->setHorizontalSpacing(10);
    paramLayout->setVerticalSpacing(6);

    m_spinChunkSize = new QSpinBox();
    m_spinChunkSize->setRange(16, 240);
    m_spinChunkSize->setValue(200);
    m_spinChunkSize->setSuffix(QStringLiteral(" 字节/包"));

    m_spinIntervalMs = new QSpinBox();
    m_spinIntervalMs->setRange(0, 5000);
    m_spinIntervalMs->setValue(20);
    m_spinIntervalMs->setSuffix(QStringLiteral(" ms"));

    paramLayout->addWidget(new QLabel(QStringLiteral("每包字节数：")), 0, 0);
    paramLayout->addWidget(m_spinChunkSize, 0, 1);
    paramLayout->addWidget(new QLabel(QStringLiteral("包间隔：")),     0, 2);
    paramLayout->addWidget(m_spinIntervalMs, 0, 3);
    paramLayout->setColumnStretch(1, 1);
    paramLayout->setColumnStretch(3, 1);

    mainLayout->addLayout(paramLayout);

    // ---- 进度条 ----
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    mainLayout->addWidget(m_progress);

    // ---- 状态 ----
    m_lblStatus = new QLabel(QStringLiteral("就绪"));
    mainLayout->addWidget(m_lblStatus);

    // ---- 日志 ----
    m_log = new QPlainTextEdit();
    m_log->setReadOnly(true);
    mainLayout->addWidget(m_log, 1);

    // ---- 按钮区 ----
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnClose = new QPushButton(QStringLiteral("关闭"));
    m_btnClose->setObjectName("secondary");
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::close);

    m_btnStart = new QPushButton(QStringLiteral("开始升级"));
    m_btnStart->setEnabled(false);
    connect(m_btnStart, &QPushButton::clicked,
            this, &OtaUpgradeDialog::onStartUpgrade);

    btnLayout->addWidget(m_btnClose);
    btnLayout->addWidget(m_btnStart);

    mainLayout->addLayout(btnLayout);
}

void OtaUpgradeDialog::setUiBusy(bool busy)
{
    m_upgrading = busy;
    m_rdoWired->setEnabled(!busy);
    m_rdoWifi->setEnabled(!busy);
    m_editWifiIp->setEnabled(!busy);
    m_editWifiPort->setEnabled(!busy);
    m_spinChunkSize->setEnabled(!busy);
    m_spinIntervalMs->setEnabled(!busy);
    m_btnSelect->setEnabled(!busy);
    m_btnStart->setEnabled(!busy && !m_fileData.isEmpty());
    m_btnClose->setText(busy ? QStringLiteral("取消") : QStringLiteral("关闭"));
}

void OtaUpgradeDialog::appendLog(const QString &text)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    m_log->appendPlainText(QString("[%1] %2").arg(ts, text));
}

bool OtaUpgradeDialog::isWifiMode() const
{
    return m_rdoWifi && m_rdoWifi->isChecked();
}

// ============================================================
// 文件
// ============================================================
void OtaUpgradeDialog::onSelectFile()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择固件文件"),
        QString(),
        QStringLiteral("Bin 文件 (*.bin);;所有文件 (*.*)"));

    if (path.isEmpty())
        return;

    loadBinFile(path);
}

bool OtaUpgradeDialog::loadBinFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法打开文件: %1").arg(path));
        return false;
    }

    m_fileData = f.readAll();
    f.close();

    if (m_fileData.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("文件为空"));
        return false;
    }

    m_filePath = path;
    m_fileCrc  = modbusCrc16(
        reinterpret_cast<const quint8 *>(m_fileData.constData()),
        m_fileData.size());

    QFileInfo info(path);
    m_lblFile->setText(QStringLiteral("文件: %1").arg(info.fileName()));
    m_lblSize->setText(QStringLiteral("大小: %1 字节").arg(m_fileData.size()));
    m_lblCrc->setText(QStringLiteral("CRC16: 0x%1")
                          .arg(m_fileCrc, 4, 16, QChar('0')).toUpper());

    appendLog(QStringLiteral("已加载: %1").arg(path));
    appendLog(QStringLiteral("大小: %1 字节, CRC16: 0x%2")
                  .arg(m_fileData.size())
                  .arg(m_fileCrc, 4, 16, QChar('0')).toUpper());

    m_btnStart->setEnabled(true);
    return true;
}

// ============================================================
// 底层传输
// ============================================================
bool OtaUpgradeDialog::openTransport()
{
    if (isWifiMode())
    {
        closeTransport();

        m_tcpSocket = new QTcpSocket(this);

        QString ip   = m_editWifiIp->text().trimmed();
        quint16 port = quint16(m_editWifiPort->text().toUInt());

        appendLog(QStringLiteral("连接 WiFi 模块 %1:%2 ...").arg(ip).arg(port));

        m_tcpSocket->connectToHost(ip, port);
        if (!m_tcpSocket->waitForConnected(3000))
        {
            appendLog(QStringLiteral("WiFi 连接失败: %1")
                          .arg(m_tcpSocket->errorString()));
            closeTransport();
            return false;
        }

        appendLog(QStringLiteral("WiFi 连接成功"));
        return true;
    }
    else
    {
        if (!m_serialPort || !m_serialPort->isOpen())
        {
            appendLog(QStringLiteral("串口未打开"));
            return false;
        }
        appendLog(QStringLiteral("使用串口: %1").arg(m_serialPort->portName()));
        return true;
    }
}

void OtaUpgradeDialog::closeTransport()
{
    if (m_tcpSocket)
    {
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
}

qint64 OtaUpgradeDialog::writeBytes(const QByteArray &data)
{
    if (isWifiMode())
    {
        if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState)
            return -1;

        qint64 n = m_tcpSocket->write(data);
        m_tcpSocket->flush();
        return n;
    }
    else
    {
        if (!m_serialPort || !m_serialPort->isOpen())
            return -1;

        qint64 n = m_serialPort->write(data);
        m_serialPort->flush();
        return n;
    }
}

QByteArray OtaUpgradeDialog::readBytes(int timeoutMs)
{
    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs)
    {
        int remain = timeoutMs - int(timer.elapsed());
        int wait   = qMin(remain, 50);

        if (isWifiMode())
        {
            if (!m_tcpSocket) break;
            if (m_tcpSocket->waitForReadyRead(wait))
                buffer.append(m_tcpSocket->readAll());
        }
        else
        {
            if (!m_serialPort) break;
            if (m_serialPort->waitForReadyRead(wait))
                buffer.append(m_serialPort->readAll());
        }

        if (buffer.size() >= 6)
            break;
    }
    return buffer;
}

// ============================================================
// OTA 流程
// ============================================================
void OtaUpgradeDialog::onStartUpgrade()
{
    if (m_fileData.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择 bin 文件"));
        return;
    }

    setUiBusy(true);
    m_progress->setValue(0);
    m_lblStatus->setText(QStringLiteral("准备连接..."));

    // ---- 0. 打开传输通道 ----
    if (!openTransport())
    {
        setUiBusy(false);
        m_lblStatus->setText(QStringLiteral("连接失败"));
        return;
    }

    // ---- 1. 发送 OTA 开始帧 00 02 00 00 + len + crc ----
    m_lblStatus->setText(QStringLiteral("发送 OTA 开始帧..."));
    QCoreApplication::processEvents();

    if (!sendOtaStartCommand())
    {
        closeTransport();
        setUiBusy(false);
        m_lblStatus->setText(QStringLiteral("发送失败"));
        return;
    }

    // ---- 2. 等待 01 02 00 00 + CRC ----
    m_lblStatus->setText(QStringLiteral("等待开始响应..."));
    QCoreApplication::processEvents();

    if (!waitOtaStartAck(1500))
    {
        closeTransport();
        setUiBusy(false);
        m_lblStatus->setText(QStringLiteral("OTA 握手失败"));
        return;
    }

    appendLog(QStringLiteral("OTA 握手成功，开始传输 bin 文件"));

    // ---- 3. 传输 bin ----
    if (!sendFirmwareData())
    {
        closeTransport();
        setUiBusy(false);
        m_lblStatus->setText(QStringLiteral("传输失败"));
        return;
    }

    // ★★★ 关键：等 1 秒，让下位机 200ms 超时 + 校验 + 恢复 ModbusTask
    appendLog(QStringLiteral("bin 发送完成，等待下位机校验..."));
    m_lblStatus->setText(QStringLiteral("等待下位机校验..."));
    QCoreApplication::processEvents();

    QThread::msleep(1000);
    QCoreApplication::processEvents();

    // ---- 4. 发送升级完成帧 ----
    m_lblStatus->setText(QStringLiteral("发送升级完成帧..."));
    QCoreApplication::processEvents();

    if (!sendOtaDoneCommand())
    {
        closeTransport();
        setUiBusy(false);
        m_lblStatus->setText(QStringLiteral("发送完成帧失败"));
        return;
    }
}

// ============================================================
// 发送 OTA 开始帧：
//   [00][02][00][00][lenH][lenL][lenHH][lenLL][crcH][crcL] + CRC
// ============================================================
bool OtaUpgradeDialog::sendOtaStartCommand()
{
    QByteArray data;
    data.append(char(0x00));   // 广播地址
    data.append(char(0x02));   // 功能码 OTA
    data.append(char(0x00));   // 子命令：开始
    data.append(char(0x00));

    /* ★ 4 字节长度（大端） */
    quint32 len = static_cast<quint32>(m_fileData.size());
    data.append(char((len >> 24) & 0xFF));
    data.append(char((len >> 16) & 0xFF));
    data.append(char((len >> 8)  & 0xFF));
    data.append(char( len        & 0xFF));

    /* ★ 2 字节整体 CRC16（大端） */
    data.append(char((m_fileCrc >> 8) & 0xFF));
    data.append(char( m_fileCrc       & 0xFF));

    QByteArray frame = ModbusRTU::buildFrame(data);

    appendLog(QStringLiteral("TX [开始]: %1")
                  .arg(QString::fromLatin1(frame.toHex(' ').toUpper())));

    if (writeBytes(frame) != frame.size())
    {
        appendLog(QStringLiteral("写失败"));
        return false;
    }
    return true;
}

// ============================================================
// 等待 01 02 00 00 + CRC
// ============================================================
bool OtaUpgradeDialog::waitOtaStartAck(int timeoutMs)
{
    QByteArray buffer = readBytes(timeoutMs);
    if (buffer.isEmpty())
    {
        appendLog(QStringLiteral("未收到开始响应"));
        return false;
    }

    appendLog(QStringLiteral("RX: %1")
                  .arg(QString::fromLatin1(buffer.toHex(' ').toUpper())));

    const QByteArray expect = QByteArray::fromHex("01020000");
    if (!buffer.contains(expect))
    {
        appendLog(QStringLiteral("响应不匹配，期望包含 01 02 00 00"));
        return false;
    }

    int idx = buffer.indexOf(expect);
    if (idx >= 0 && buffer.size() >= idx + 6)
    {
        QByteArray resp = buffer.mid(idx, 6);
        quint16 calc = modbusCrc16(
            reinterpret_cast<const quint8 *>(resp.constData()), 4);
        quint16 recv = (quint8)resp[4] | ((quint8)resp[5] << 8);
        if (calc != recv)
        {
            appendLog(QStringLiteral("响应 CRC 错误"));
            return false;
        }
        appendLog(QStringLiteral("开始响应 CRC 通过"));
    }

    return true;
}

// ============================================================
// 传输 bin
// ============================================================
bool OtaUpgradeDialog::sendFirmwareData()
{
    const int chunkSize  = m_spinChunkSize->value();
    const int intervalMs = m_spinIntervalMs->value();

    const int total = m_fileData.size();
    int     sent = 0;
    quint16 seq  = 0;

    appendLog(QStringLiteral("开始传输：%1 字节，每包 %2 字节，间隔 %3 ms")
                  .arg(total).arg(chunkSize).arg(intervalMs));

    while (sent < total)
    {
        int n = qMin(chunkSize, total - sent);

        QByteArray pkt;
        pkt.append(char(0x00));
        pkt.append(char(0x02));
        pkt.append(char(0x01));
        pkt.append(char((seq >> 8) & 0xFF));
        pkt.append(char(seq & 0xFF));
        pkt.append(char((n >> 8) & 0xFF));
        pkt.append(char(n & 0xFF));
        pkt.append(m_fileData.constData() + sent, n);

        QByteArray frame = ModbusRTU::buildFrame(pkt);

        if (writeBytes(frame) != frame.size())
        {
            appendLog(QStringLiteral("第 %1 包写入失败").arg(seq));
            return false;
        }

        sent += n;
        seq++;

        int percent = total > 0 ? (sent * 100 / total) : 100;
        m_progress->setValue(percent);
        m_lblStatus->setText(QStringLiteral("发送中... %1% (%2/%3 字节)")
                                 .arg(percent).arg(sent).arg(total));
        QCoreApplication::processEvents();

        if (intervalMs > 0 && sent < total)
        {
            QThread::msleep(intervalMs);
        }
    }

    appendLog(QStringLiteral("固件数据发送完成，共 %1 包").arg(seq));
    return true;
}

// ============================================================
// 发送 00 02 00 01 + CRC
// ============================================================
bool OtaUpgradeDialog::sendOtaDoneCommand()
{
    QByteArray data;
    data.append(char(0x00));
    data.append(char(0x02));
    data.append(char(0x00));
    data.append(char(0x01));

    QByteArray frame = ModbusRTU::buildFrame(data);

    appendLog(QStringLiteral("TX [完成]: %1")
                  .arg(QString::fromLatin1(frame.toHex(' ').toUpper())));

    if (writeBytes(frame) != frame.size())
    {
        appendLog(QStringLiteral("写失败"));
        return false;
    }
    return true;
}

// ============================================================
// 等待 01 02 00 01 + CRC
// ============================================================
bool OtaUpgradeDialog::waitOtaDoneAck(int timeoutMs)
{
    QByteArray buffer = readBytes(timeoutMs);
    if (buffer.isEmpty())
    {
        appendLog(QStringLiteral("未收到完成响应"));
        return false;
    }

    appendLog(QStringLiteral("RX: %1")
                  .arg(QString::fromLatin1(buffer.toHex(' ').toUpper())));

    const QByteArray expect = QByteArray::fromHex("01020001");
    if (!buffer.contains(expect))
    {
        appendLog(QStringLiteral("响应不匹配，期望包含 01 02 00 01"));
        return false;
    }

    int idx = buffer.indexOf(expect);
    if (idx >= 0 && buffer.size() >= idx + 6)
    {
        QByteArray resp = buffer.mid(idx, 6);
        quint16 calc = modbusCrc16(
            reinterpret_cast<const quint8 *>(resp.constData()), 4);
        quint16 recv = (quint8)resp[4] | ((quint8)resp[5] << 8);
        if (calc != recv)
        {
            appendLog(QStringLiteral("完成响应 CRC 错误"));
            return false;
        }
        appendLog(QStringLiteral("完成响应 CRC 通过"));
    }

    return true;
}