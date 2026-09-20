#include "mainwindow.h"
#include "titlebar.h"
#include "settingspanel.h"
#include "backgrounddialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>
#include <QPushButton>
#include <QSerialPort>
#include <QDebug>

static const int kTitleBarHeight = 56;
static const int kCornerRadius   = 14;

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

    setWindowTitle(QStringLiteral("压风式散热器v0.0.1"));
    updateLayout();
}

MainWindow::~MainWindow() {}

// ============================================================
// 初始化
// ============================================================
void MainWindow::setupUI()
{
    // ---- 1. 状态栏 ----
    m_titleBar = new TitleBar(this);
    m_titleBar->setTitle(QStringLiteral("压风式散热器v0.0.1"));

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
    connect(m_titleBar, &TitleBar::serialPortChanged,
            this, &MainWindow::onSerialPortChanged);

    // ---- 2. 设置菜单 ----
    m_settingsMenu = new SettingsPanel(this);
    connect(m_settingsMenu, &SettingsPanel::backgroundSettingsRequested,
            this, &MainWindow::onOpenBackgroundDialog);

    // ---- 3. 串口对象 ⭐ ----
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &MainWindow::onSerialDataReceived);
}

// ============================================================
// 状态栏位置
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
    if (checked) {
        qDebug() << "散热器已启动";

        QString port = m_titleBar->currentSerialPort();
        if (port.isEmpty()) {
            qDebug() << "⚠ 请先选择串口";
        } else {
            openSerialPort(port);
        }
    } else {
        qDebug() << "散热器已停止";
        closeSerialPort();
    }
}

// ============================================================
// 串口切换
// ============================================================
void MainWindow::onSerialPortChanged(const QString &portName)
{
    qDebug() << "切换到串口:" << portName;

    // 如果已经启动，先关闭旧串口再打开新串口
    if (m_titleBar->isPowerOn()) {
        closeSerialPort();
        openSerialPort(portName);
    }
}

// ============================================================
// ⭐ 收到串口数据：触发按钮闪烁
// ============================================================
void MainWindow::onSerialDataReceived()
{
    if (!m_serialPort) return;

    // 读取全部数据（实际项目里可以解析协议）
    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) return;

    // qDebug() << "收到数据:" << data.toHex(' ');

    // ⭐ 让标题栏的电源按钮闪一下
    m_titleBar->flashPowerButton();
}

// ============================================================
// 打开串口
// ============================================================
void MainWindow::openSerialPort(const QString &portName)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "串口已打开:" << portName;
    } else {
        qDebug() << "串口打开失败:" << m_serialPort->errorString();
    }
}

// ============================================================
// 关闭串口
// ============================================================
void MainWindow::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        qDebug() << "串口已关闭";
    }
}