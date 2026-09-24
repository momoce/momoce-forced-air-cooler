#include "mainwindow.h"
#include "titlebar.h"
#include "settingspanel.h"
#include "backgrounddialog.h"
#include "otaupgradedialog.h"      // OTA升级子窗口接收槽
#include "modbus_rtu.h"
#include "devicepromptwidget.h"


#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPushButton>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>
#include <QTimer>
#include <QDebug>
#include <QMessageBox>

static const int kTitleBarHeight = 56;
static const int kCornerRadius   = 14;

static QString hexDump(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex(' ')).toUpper();
}

// ============================================================
// 构造函数
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_backgroundColor(45, 45, 48)
    , m_opacity(255)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    setupUI();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect geo = screen->geometry();
        resize(geo.width() / 2, geo.height() / 2);
        move(geo.x() + (geo.width()  - width())  / 2,
             geo.y() + (geo.height() - height()) / 2);
    }

    setWindowTitle(QStringLiteral("压风式散热器v0.0.2"));
    updateLayout();
}

MainWindow::~MainWindow()
{
    stopHeartbeat();
    closeSerialPort();
}

// ============================================================
// 初始化
// ============================================================
void MainWindow::setupUI()
{
    // ---- 1. 标题栏 ----
    m_titleBar = new TitleBar(this);
    m_titleBar->setTitle(QStringLiteral("压风式散热器v0.0.2"));

    connect(m_titleBar, &TitleBar::gearClicked,
            this, &MainWindow::onGearClicked);
    connect(m_titleBar, &TitleBar::minimizeClicked,
            this, &MainWindow::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeClicked, this, [this]{
        if (isMaximized()) showNormal();
        else               showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeClicked,
            this, &MainWindow::close);
    connect(m_titleBar, &TitleBar::powerToggled,
            this, &MainWindow::onPowerToggled);

    // ---- 2. 设置菜单 ----

    m_settingsMenu = new SettingsPanel(this);
    connect(m_settingsMenu, &SettingsPanel::backgroundSettingsRequested,
            this, &MainWindow::onOpenBackgroundDialog);

    // OTA 升级连接
    connect(m_settingsMenu, &SettingsPanel::otaUpgradeRequested,
            this, &MainWindow::onOpenOtaUpgrade);

    // ---- 3. 串口对象 ----
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &MainWindow::onSerialDataReceived);

    // ---- 4. Modbus 扫描器 ----
    m_scanner = new ModbusScanner(this);
    connect(m_scanner, &ModbusScanner::deviceFound,
            this, &MainWindow::onDeviceFound);
    connect(m_scanner, &ModbusScanner::scanFinished,
            this, &MainWindow::onScanFinished);
    connect(m_scanner, &ModbusScanner::portTrying,
            this, [](const QString &p){ qDebug() << "[Scan] 尝试端口：" << p; });
    connect(m_scanner, &ModbusScanner::portTimeout,
            this, [](const QString &p){ qDebug() << "[Scan] 无响应端口：" << p; });

    // ---- 5. 设备提示卡片 ----
    m_devicePrompt = new DevicePromptWidget(this);
    m_devicePrompt->hide();

    connect(m_devicePrompt, &DevicePromptWidget::connectRequested,
            this, &MainWindow::onConnectRequested);

    // ---- 6. 串口轮询定时器 ----
    m_portCheckTimer = new QTimer(this);
    m_portCheckTimer->setInterval(1000);
    connect(m_portCheckTimer, &QTimer::timeout,
            this, &MainWindow::checkForNewPorts);
    m_portCheckTimer->start();

    // ---- 7. 连接握手超时 ----
    m_connectTimeout = new QTimer(this);
    m_connectTimeout->setSingleShot(true);
    m_connectTimeout->setInterval(800);
    connect(m_connectTimeout, &QTimer::timeout,
            this, &MainWindow::onConnectTimeout);

    // ---- 8. 断开握手超时 ----
    m_disconnectTimeout = new QTimer(this);
    m_disconnectTimeout->setSingleShot(true);
    m_disconnectTimeout->setInterval(800);
    connect(m_disconnectTimeout, &QTimer::timeout,
            this, &MainWindow::onDisconnectTimeout);

    // ---- 9. 心跳定时器（200ms） ----
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(200);
    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &MainWindow::onHeartbeatTimeout);

    m_knownPorts = allAvailablePorts();


}

// ============================================================
// 布局
// ============================================================
void MainWindow::updateLayout()
{
    if (!m_titleBar) return;
    m_titleBar->setGeometry(0, 0, width(), kTitleBarHeight);
    m_titleBar->raise();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateLayout();
    update();

    if (m_devicePrompt) {
        m_devicePrompt->setGeometry(
            0, kTitleBarHeight,
            width(), height() - kTitleBarHeight);
    }
}

// ============================================================
// 绘制
// ============================================================
void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(rect()), kCornerRadius, kCornerRadius);
    painter.setClipPath(clipPath);

    QLinearGradient grad(0, 0, 0, kTitleBarHeight);
    grad.setColorAt(0.0, QColor(250, 250, 250));
    grad.setColorAt(1.0, QColor(235, 235, 235));
    painter.fillRect(QRect(0, 0, width(), kTitleBarHeight), grad);

    painter.setPen(QPen(QColor(0, 0, 0, 30), 1));
    painter.drawLine(0, kTitleBarHeight - 1, width(), kTitleBarHeight - 1);

    QRect contentRect(0, kTitleBarHeight, width(), height() - kTitleBarHeight);
    if (!contentRect.isEmpty()) {
        painter.fillRect(contentRect, m_backgroundColor);

        if (!m_backgroundImage.isNull()) {
            painter.setOpacity(m_opacity / 255.0);
            QPixmap scaled = m_backgroundImage.scaled(
                size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (scaled.width()  - width())  / 2;
            int y = (scaled.height() - height()) / 2;
            painter.drawPixmap(0, 0, scaled, x, y, width(), height());
            painter.setOpacity(1.0);
        }
    }

    painter.setClipping(false);
    painter.setPen(QPen(QColor(0, 0, 0, 40), 1));
    QPainterPath borderPath;
    borderPath.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                              kCornerRadius, kCornerRadius);
    painter.drawPath(borderPath);
}

// ============================================================
// 齿轮
// ============================================================
void MainWindow::onGearClicked()
{
    if (!m_settingsMenu) return;

    QPushButton *gear = m_titleBar->gearButton();
    if (!gear) return;

    QPoint gearBottomLeft = gear->mapToGlobal(
        QPoint(0, gear->height() + 4));

    int menuWidth = m_settingsMenu->sizeHint().width();
    int x = gearBottomLeft.x() + gear->width() - menuWidth;
    int y = gearBottomLeft.y();

    m_settingsMenu->popup(QPoint(x, y));
}

// ============================================================
// 背景设置
// ============================================================
void MainWindow::onOpenBackgroundDialog()
{
    BackgroundDialog dlg(m_backgroundColor,
                         m_backgroundImage,
                         m_opacity,
                         this);

    if (dlg.exec() != QDialog::Accepted) return;

    m_backgroundColor = dlg.selectedColor();
    m_backgroundImage = dlg.selectedImage();
    m_opacity         = dlg.selectedOpacity();

    update();
}

// ============================================================
// 电源开关
// ============================================================
void MainWindow::onPowerToggled(bool checked)
{
    if (checked && m_deviceConnected)
        return;

    // 1. 已连接串口被拔掉
    if (m_deviceConnected && !isPortAvailable(m_currentPortName)) {
        qDebug() << "[状态] 已连接串口被拔掉：" << m_currentPortName;

        stopHeartbeat();
        closeSerialPort();      // 内部隐藏子窗口
        m_deviceFound = false;
        m_foundPortName.clear();

        QStringList all = allAvailablePorts();
        m_knownPorts = all;
        if (!all.isEmpty())
            startScan(all);
        return;
    }

    // 2. 开关关闭：已连接 → 发送断开帧
    if (!checked) {
        if (m_deviceConnected && m_serialPort && m_serialPort->isOpen()) {
            m_disconnectPending = true;
            m_rxBuffer.clear();
            m_disconnectTimeout->start();

            QByteArray frame;
            frame.append(char(0x00));
            frame.append(char(0x01));
            frame.append(char(0x01));
            frame.append(char(0x01));
            frame = ModbusRTU::buildFrame(frame);

            qDebug() << "[TX][断开]" << hexDump(frame)
                     << " 端口：" << m_currentPortName;
            m_serialPort->write(frame);
            m_serialPort->flush();
        }
        return;
    }

    // 3. 开关打开：优先连已找到的设备
    if (m_deviceFound && !m_deviceConnected) {
        if (isPortAvailable(m_foundPortName)) {
            stopHeartbeat();
            startConnectHandshake(m_foundPortName);
        } else {
            m_deviceFound = false;
            m_foundPortName.clear();

            if (m_devicePrompt)
                m_devicePrompt->hide();

            if (m_titleBar && m_titleBar->powerButton())
                m_titleBar->powerButton()->setChecked(false);

            QStringList all = allAvailablePorts();
            m_knownPorts = all;
            if (!all.isEmpty())
                startScan(all);
        }
        return;
    }

    // 4. 没找到过设备：扫描全部
    if (m_devicePrompt)
        m_devicePrompt->hide();

    if (m_titleBar && m_titleBar->powerButton())
        m_titleBar->powerButton()->setChecked(false);

    QStringList all = allAvailablePorts();
    m_knownPorts = all;
    qDebug() << "[Scan] 按下开关，扫描所有串口：" << all;
    if (!all.isEmpty())
        startScan(all);
}

// ============================================================
// 收到串口数据
// ============================================================
void MainWindow::onSerialDataReceived()
{
    if (!m_serialPort) return;

    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) return;

    qDebug() << "[RX]" << hexDump(data)
             << " 端口：" << m_currentPortName;

    m_rxBuffer.append(data);

    // ⭐ 心跳收到任何响应都算有效
    if (m_heartbeatTimer && m_heartbeatTimer->isActive())
        m_heartbeatResponseReceived = true;

    // ---------- 正在等连接响应 ----------
    if (m_connectPending) {
        const QByteArray expect = QByteArray::fromHex("01010001");

        if (m_rxBuffer.contains(expect)) {
            qDebug() << "[握手] 收到连接响应，成功";

            m_connectPending = false;
            m_connectTimeout->stop();
            m_rxBuffer.clear();

            m_deviceConnected = true;

            if (m_titleBar && m_titleBar->powerButton())
                m_titleBar->powerButton()->setChecked(true);

            return;
        }

        if (m_rxBuffer.size() > 256)
            m_rxBuffer.clear();
        return;
    }

    // ---------- 正在等断开响应 ----------
    if (m_disconnectPending) {
        const QByteArray expect = QByteArray::fromHex("01010101");

        if (m_rxBuffer.contains(expect)) {
            qDebug() << "[握手] 收到断开响应，成功";

            m_disconnectPending = false;
            m_disconnectTimeout->stop();
            m_rxBuffer.clear();

            closeSerialPort();
            return;
        }

        if (m_rxBuffer.size() > 256)
            m_rxBuffer.clear();
        return;
    }

    // ---------- 心跳期间：不清缓冲 ----------
    if (m_heartbeatTimer && m_heartbeatTimer->isActive())
        return;

    // ---------- 正常通信 ----------
    if (m_deviceConnected && m_titleBar)
        m_titleBar->flashPowerButton();

    m_rxBuffer.clear();
}

// ============================================================
// 统一握手入口
// ============================================================
void MainWindow::startConnectHandshake(const QString &portName)
{
    if (portName.isEmpty())
        return;

    qDebug() << "[握手] 开始连接：" << portName;

    openSerialPort(portName);

    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "[握手] 串口打开失败，无法发送连接帧";
        return;
    }

    m_connectPending = true;
    m_rxBuffer.clear();
    m_connectTimeout->start();

    QByteArray frame;
    frame.append(char(0x00));
    frame.append(char(0x01));
    frame.append(char(0x00));
    frame.append(char(0x01));
    frame = ModbusRTU::buildFrame(frame);

    qDebug() << "[TX][连接]" << hexDump(frame)
             << " 端口：" << portName;
    m_serialPort->write(frame);
    m_serialPort->flush();
}

// ============================================================
// 卡片点击后
// ============================================================
void MainWindow::onConnectRequested(const QString &portName)
{
    qDebug() << "[状态] 用户点击卡片连接：" << portName;
    stopHeartbeat();
    startConnectHandshake(portName);
}

// ============================================================
// ⭐ 打开串口
// ============================================================
void MainWindow::openSerialPort(const QString &portName)
{
    if (m_serialPort->isOpen())
        m_serialPort->close();

    m_rxBuffer.clear();

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "[串口] 已打开：" << portName;
        m_currentPortName = portName;
    } else {
        qWarning() << "[串口] 打开失败：" << m_serialPort->errorString();

        m_deviceConnected = false;
        m_currentPortName.clear();

        if (m_titleBar && m_titleBar->powerButton())
            m_titleBar->powerButton()->setChecked(false);
    }
}

// ============================================================
// ⭐ 关闭串口：内部统一隐藏子窗口
// ============================================================
void MainWindow::closeSerialPort()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
        qDebug() << "[串口] 已关闭";
    }

    m_deviceConnected   = false;
    m_currentPortName.clear();

    m_connectPending    = false;
    m_disconnectPending = false;
    if (m_connectTimeout)    m_connectTimeout->stop();
    if (m_disconnectTimeout) m_disconnectTimeout->stop();

    // ⭐ 统一隐藏子窗口
    if (m_devicePrompt)
        m_devicePrompt->hide();

    if (m_titleBar && m_titleBar->powerButton())
        m_titleBar->powerButton()->setChecked(false);
}

// ============================================================
// ⭐ 心跳
// ============================================================
void MainWindow::startHeartbeat()
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        qWarning() << "[心跳] 串口未打开，无法启动心跳";
        return;
    }

    m_heartbeatMissCount = 0;
    m_heartbeatResponseReceived = false;
    m_heartbeatFirst = true;
    m_rxBuffer.clear();

    m_heartbeatTimer->start();
    qDebug() << "[心跳] 启动（200ms 一次）";
}

void MainWindow::stopHeartbeat()
{
    if (m_heartbeatTimer && m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
        qDebug() << "[心跳] 停止";
    }
    m_heartbeatMissCount = 0;
    m_heartbeatResponseReceived = false;
    m_heartbeatFirst = true;
}

void MainWindow::onHeartbeatTimeout()
{
    if (!m_serialPort || !m_serialPort->isOpen())
        return;

    if (!m_heartbeatFirst) {
        if (m_heartbeatResponseReceived) {
            m_heartbeatMissCount = 0;
        } else {
            m_heartbeatMissCount++;
            qDebug() << "[心跳] 未响应，累计：" << m_heartbeatMissCount;

            if (m_heartbeatMissCount >= 3) {
                qDebug() << "[心跳] 设备丢失，关闭子窗口";
                onHeartbeatLost();
                return;
            }
        }
    }
    m_heartbeatFirst = false;
    m_heartbeatResponseReceived = false;

    QByteArray frame;
    frame.append(char(0x00));
    frame.append(char(0x01));
    frame.append(char(0x00));
    frame.append(char(0x00));
    frame = ModbusRTU::buildFrame(frame);

    qDebug() << "[TX][心跳]" << hexDump(frame)
             << " 端口：" << m_currentPortName;
    m_serialPort->write(frame);
    m_serialPort->flush();
}

void MainWindow::onHeartbeatLost()
{
    qDebug() << "[心跳] 设备丢失，关闭子窗口";

    stopHeartbeat();
    closeSerialPort();      // 内部会隐藏子窗口、复位按钮

    m_deviceFound = false;
    m_foundPortName.clear();
}

// ============================================================
// 工具函数
// ============================================================
QStringList MainWindow::allAvailablePorts() const
{
    QStringList list;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports)
        list << info.portName();
    return list;
}

bool MainWindow::isPortAvailable(const QString &portName) const
{
    if (portName.isEmpty()) return false;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports) {
        if (info.portName() == portName)
            return true;
    }
    return false;
}

// ============================================================
// 统一扫描入口
// ============================================================
void MainWindow::startScan(const QStringList &ports)
{
    if (ports.isEmpty()) {
        qDebug() << "[Scan] 没有可扫描的串口";
        return;
    }
    if (m_scanInProgress) {
        qDebug() << "[Scan] 扫描进行中，跳过本次请求";
        return;
    }

    if (m_devicePrompt)
        m_devicePrompt->hide();

    m_scanInProgress = true;

    qDebug() << "[Scan] 开始扫描：" << ports;

    m_scanner->startScan(
        ports,
        0x00, 0x01,
        QByteArray(2, '\0'),
        QByteArray::fromHex("0101"),
        6,
        200, 115200);
}

// ============================================================
// showEvent：启动时扫一次全部
// ============================================================
void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    static bool started = false;
    if (started) return;
    started = true;

    QTimer::singleShot(150, this, [this]{
        if (!m_scanner) return;

        m_knownPorts = allAvailablePorts();
        qDebug() << "[Scan] 启动扫描，串口列表：" << m_knownPorts;

        if (!m_knownPorts.isEmpty())
            startScan(m_knownPorts);
    });
}

// ============================================================
// ⭐ 定时器：检测新串口 / 检测设备被拔
// ============================================================
void MainWindow::checkForNewPorts()
{
    if (m_scanInProgress)
        return;

    QStringList currentPorts = allAvailablePorts();

    // 1. 已连接的串口被拔掉
    if (m_deviceConnected && !currentPorts.contains(m_currentPortName)) {
        qDebug() << "[状态] 连接中串口被拔掉：" << m_currentPortName;

        stopHeartbeat();
        closeSerialPort();      // 内部隐藏子窗口、复位按钮
        m_deviceFound = false;
        m_foundPortName.clear();
    }

    // ⭐ 2. 扫描到了设备但还没连接，设备已被拔掉
    if (m_deviceFound && !m_deviceConnected
        && !m_foundPortName.isEmpty()
        && !currentPorts.contains(m_foundPortName)) {

        qDebug() << "[状态] 已找到的设备被拔掉：" << m_foundPortName;

        stopHeartbeat();
        closeSerialPort();      // 内部隐藏子窗口、复位按钮
        m_deviceFound = false;
        m_foundPortName.clear();
    }

    // 3. 找新插入的串口
    QStringList newPorts;
    for (const QString &p : currentPorts) {
        if (!m_knownPorts.contains(p))
            newPorts << p;
    }

    m_knownPorts = currentPorts;

    if (m_deviceConnected || m_deviceFound)
        return;

    if (!newPorts.isEmpty()) {
        qDebug() << "[Scan] 检测到新串口：" << newPorts;
        startScan(newPorts);
    }
}

// ============================================================
// 扫描到设备
// ============================================================
void MainWindow::onDeviceFound(const QString &portName)
{
    qDebug() << "[Scan] 发现设备：" << portName;

    m_foundPortName = portName;
    m_deviceFound = true;

    if (m_scanner)
        m_scanner->stopScan();

    // 显示子窗口
    if (m_devicePrompt)
        m_devicePrompt->setDeviceFound(portName);

    // 延迟一下再打开串口 + 启动心跳
    // （等扫描子线程真正退出，释放串口）
    QTimer::singleShot(200, this, [this, portName]{
        if (!m_deviceFound || m_foundPortName != portName)
            return;

        if (m_connectPending || m_deviceConnected)
            return;

        // ⭐ 再次确认串口还存在
        if (!isPortAvailable(portName)) {
            qDebug() << "[Scan] 延迟后串口已不存在：" << portName;
            closeSerialPort();
            m_deviceFound = false;
            m_foundPortName.clear();
            return;
        }

        openSerialPort(portName);
        if (m_serialPort && m_serialPort->isOpen())
            startHeartbeat();
    });
}

// ============================================================
// 扫描结束
// ============================================================
void MainWindow::onScanFinished()
{
    m_scanInProgress = false;
    qDebug() << "[Scan] 扫描结束，found =" << m_deviceFound
             << " connected =" << m_deviceConnected;

    // ⭐ 未找到设备时：只隐藏，不显示空白子窗口
    if (!m_deviceFound && !m_deviceConnected && m_devicePrompt)
        m_devicePrompt->hide();
}

// ============================================================
// 连接超时
// ============================================================
void MainWindow::onConnectTimeout()
{
    if (!m_connectPending)
        return;

    qDebug() << "[握手] 连接超时，未收到响应";

    m_connectPending = false;
    m_rxBuffer.clear();

    closeSerialPort();

    QMessageBox::warning(
        this,
        QStringLiteral("连接失败"),
        QStringLiteral("设备未响应，请重试。"));
}

// ============================================================
// 断开超时
// ============================================================
void MainWindow::onDisconnectTimeout()
{
    if (!m_disconnectPending)
        return;

    qDebug() << "[握手] 断开超时，未收到响应，强制关闭串口";

    m_disconnectPending = false;
    m_rxBuffer.clear();

    closeSerialPort();
}



//OTA升级子菜单的相关执行函数
void MainWindow::onOpenOtaUpgrade()
{
    // 1. 暂停心跳
    bool wasHeartbeatActive = m_heartbeatTimer && m_heartbeatTimer->isActive();
    stopHeartbeat();

    // 2. 断开 readyRead，避免 MainWindow 抢 OTA 响应
    if (m_serialPort) {
        disconnect(m_serialPort, &QSerialPort::readyRead,
                   this, &MainWindow::onSerialDataReceived);
    }

    // 3. 打开 OTA 对话框（把串口传进去，WiFi 模式内部会新建 TCP）
    OtaUpgradeDialog dlg(m_serialPort, this);
    dlg.exec();

    // 4. 恢复 readyRead 和心跳
    if (m_serialPort) {
        connect(m_serialPort, &QSerialPort::readyRead,
                this, &MainWindow::onSerialDataReceived);
    }

    if (wasHeartbeatActive && m_serialPort && m_serialPort->isOpen()) {
        startHeartbeat();
    }
}