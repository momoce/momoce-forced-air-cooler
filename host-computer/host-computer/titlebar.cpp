#include "titlebar.h"
#include <QPainter>
#include <QPainterPath>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>
#include <QIcon>
#include <QTimer>
#include <QSerialPortInfo>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ⭐ 闪烁参数（想调就改这里）
static const int kFlashDuration    = 50;   // 深蓝色持续 100ms
static const int kCooldownDuration = 50;   // 浅蓝色至少持续 100ms

// ⭐ 按钮颜色
static const QString kColorOff        = "#cccccc";   // 关闭：灰
static const QString kColorOffHover   = "#bfbfbf";
static const QString kColorOffBorder  = "#b0b0b0";

static const QString kColorOn         = "#64B5F6";   // 启动：浅蓝
static const QString kColorOnHover    = "#42A5F5";
static const QString kColorOnBorder   = "#42A5F5";

static const QString kColorFlash      = "#1976D2";   // 收到数据：深蓝
static const QString kColorFlashHover = "#1565C0";
static const QString kColorFlashBorder= "#0D47A1";

// ============================================================
// 自绘齿轮图标
// ============================================================
static QPixmap makeGearPixmap(int size, const QColor &color)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(size / 2.0, size / 2.0);

    const int   teeth  = 8;
    const qreal outerR = size * 0.46;
    const qreal innerR = size * 0.34;
    const qreal holeR  = size * 0.14;

    QPainterPath path;
    for (int i = 0; i < teeth * 2; ++i) {
        qreal angle = i * M_PI / teeth - M_PI / 2;
        qreal r = (i % 2 == 0) ? outerR : innerR;
        QPointF pt(r * std::cos(angle), r * std::sin(angle));
        if (i == 0) path.moveTo(pt);
        else        path.lineTo(pt);
    }
    path.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(path);

    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.setBrush(Qt::transparent);
    p.drawEllipse(QPointF(0, 0), holeR, holeR);

    return pix;
}

// ============================================================
// 构造函数
// ============================================================
TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("customTitleBar");
    setAttribute(Qt::WA_StyledBackground, true);

    setStyleSheet(
        "#customTitleBar {"
        "    background-color: transparent;"
        "    border: none;"
        "}");

    // ⭐ 初始化闪烁定时器
    m_flashTimer = new QTimer(this);
    m_flashTimer->setSingleShot(true);
    m_flashTimer->setInterval(kFlashDuration);
    connect(m_flashTimer, &QTimer::timeout, this, &TitleBar::onFlashTimeout);

    m_cooldownTimer = new QTimer(this);
    m_cooldownTimer->setSingleShot(true);
    m_cooldownTimer->setInterval(kCooldownDuration);
    connect(m_cooldownTimer, &QTimer::timeout, this, &TitleBar::onCooldownTimeout);

    setupUI();
}

// ============================================================
// 初始化所有子控件
// ============================================================
void TitleBar::setupUI()
{
    // ---------------- 1. 标题 ----------------
    m_titleLabel = new QLabel(QStringLiteral("压风式散热器"), this);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #333; font-size: 14px; font-weight: bold; }");

    // ---------------- 2. 圆形开关按钮 ----------------
    m_powerButton = new QPushButton(this);
    m_powerButton->setFixedSize(28, 28);
    m_powerButton->setCheckable(true);
    m_powerButton->setChecked(false);
    m_powerButton->setCursor(Qt::PointingHandCursor);
    m_powerButton->setToolTip(QStringLiteral("启动 / 停止"));
    m_powerButton->setToolTipDuration(2000);

    // 初始样式由 updatePowerButtonStyle 设置
    updatePowerButtonStyle();

    // ⭐ 状态变化：更新样式 + 复位闪烁状态
    connect(m_powerButton, &QPushButton::toggled, this, [this](bool checked){
        if (!checked) {
            // 关闭电源：停止所有闪烁
            m_flashing     = false;
            m_pendingFlash = false;
            m_flashTimer->stop();
            m_cooldownTimer->stop();
        }
        updatePowerButtonStyle();
    });

    // 对外发信号
    connect(m_powerButton, &QPushButton::toggled,
            this, &TitleBar::powerToggled);

    // ---------------- 3. 串口选择按钮 ----------------
    m_serialButton = new QPushButton(this);
    m_serialButton->setFixedHeight(28);
    m_serialButton->setMinimumWidth(130);
    m_serialButton->setCursor(Qt::PointingHandCursor);
    m_serialButton->setText(QStringLiteral("选择串口"));
    m_serialButton->setToolTip(QStringLiteral("选择串口"));
    m_serialButton->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 200);"
        "    border: 1px solid rgba(0, 0, 0, 40);"
        "    border-radius: 6px;"
        "    padding: 0 24px 0 10px;"
        "    font-size: 12px;"
        "    color: #333;"
        "    text-align: left;"
        "}"
        "QPushButton:hover   { background-color: rgba(255, 255, 255, 240); }"
        "QPushButton:pressed { background-color: rgba(220, 220, 220, 240); }");

    m_serialMenu = new QMenu(this);
    m_serialMenu->setStyleSheet(
        "QMenu {"
        "    background-color: rgba(255, 255, 255, 245);"
        "    border: 1px solid rgba(0,0,0,30);"
        "    border-radius: 6px;"
        "    padding: 4px;"
        "    font-size: 12px;"
        "}"
        "QMenu::item {"
        "    padding: 6px 24px 6px 12px;"
        "    border-radius: 4px;"
        "    color: #333;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: rgba(0,0,0,20);"
        "    margin: 4px 8px;"
        "}");

    connect(m_serialButton, &QPushButton::clicked,
            this, &TitleBar::onSerialButtonClicked);

    // ---------------- 4. 齿轮按钮 ----------------
    m_gearButton = new QPushButton(this);
    m_gearButton->setFixedSize(36, 36);
    m_gearButton->setCursor(Qt::PointingHandCursor);
    m_gearButton->setToolTip(QStringLiteral("设置"));
    m_gearButton->setToolTipDuration(2000);
    m_gearButton->setIcon(QIcon(makeGearPixmap(20, QColor(70, 70, 70))));
    m_gearButton->setIconSize(QSize(20, 20));
    m_gearButton->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 18px;"
        "}"
        "QPushButton:hover   { background-color: rgba(0,0,0,25); }"
        "QPushButton:pressed { background-color: rgba(0,0,0,50); }");
    connect(m_gearButton, &QPushButton::clicked,
            this, &TitleBar::gearClicked);

    // ---------------- 5. 最小化 ----------------
    m_minButton = new QPushButton(QStringLiteral("—"), this);
    m_minButton->setFixedSize(36, 36);
    m_minButton->setCursor(Qt::PointingHandCursor);
    m_minButton->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent; border: none;"
        "    font-size: 14px; color: #333; border-radius: 18px;"
        "}"
        "QPushButton:hover   { background-color: rgba(0,0,0,25); }"
        "QPushButton:pressed { background-color: rgba(0,0,0,50); }");
    connect(m_minButton, &QPushButton::clicked,
            this, &TitleBar::minimizeClicked);

    // ---------------- 6. 最大化 ----------------
    m_maxButton = new QPushButton(QStringLiteral("□"), this);
    m_maxButton->setFixedSize(36, 36);
    m_maxButton->setCursor(Qt::PointingHandCursor);
    m_maxButton->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent; border: none;"
        "    font-size: 14px; color: #333; border-radius: 18px;"
        "}"
        "QPushButton:hover   { background-color: rgba(0,0,0,25); }"
        "QPushButton:pressed { background-color: rgba(0,0,0,50); }");
    connect(m_maxButton, &QPushButton::clicked,
            this, &TitleBar::maximizeClicked);

    // ---------------- 7. 关闭 ----------------
    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setFixedSize(36, 36);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent; border: none;"
        "    font-size: 18px; color: #333; border-radius: 18px;"
        "}"
        "QPushButton:hover   { background-color: #e81123; color: white; }"
        "QPushButton:pressed { background-color: #c50f1f; color: white; }");
    connect(m_closeButton, &QPushButton::clicked,
            this, &TitleBar::closeClicked);

    // ---------------- 布局 ----------------
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 8, 0);
    layout->setSpacing(8);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_powerButton);
    layout->addWidget(m_serialButton);
    layout->addStretch();
    layout->addWidget(m_gearButton);
    layout->addWidget(m_minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(m_closeButton);
}

// ============================================================
// ⭐ 更新电源按钮样式
// ============================================================
void TitleBar::updatePowerButtonStyle()
{
    bool on = m_powerButton->isChecked();

    QString bg, hoverBg, border;
    if (!on) {
        // 关闭状态：灰
        bg      = kColorOff;
        hoverBg = kColorOffHover;
        border  = kColorOffBorder;
    } else if (m_flashing) {
        // 启动 + 正在闪：深蓝
        bg      = kColorFlash;
        hoverBg = kColorFlashHover;
        border  = kColorFlashBorder;
    } else {
        // 启动 + 正常：浅蓝
        bg      = kColorOn;
        hoverBg = kColorOnHover;
        border  = kColorOnBorder;
    }

    m_powerButton->setStyleSheet(QString(
                                     "QPushButton {"
                                     "    background-color: %1;"
                                     "    border: 2px solid %2;"
                                     "    border-radius: 14px;"
                                     "}"
                                     "QPushButton:hover { background-color: %3; }"
                                     ).arg(bg, border, hoverBg));
}

// ============================================================
// ⭐ 收到串口数据时调用：触发闪烁
// ============================================================
void TitleBar::flashPowerButton()
{
    if (!m_powerButton->isChecked()) return;   // 没启动不闪

    if (m_flashing) return;                    // 已经在深蓝色状态

    if (m_cooldownTimer->isActive()) {
        // 浅蓝色阶段还没结束，记住有闪烁请求
        m_pendingFlash = true;
        return;
    }

    // 立即开始闪烁
    startFlash();
}

// ============================================================
// ⭐ 开始一次深蓝色闪烁
// ============================================================
void TitleBar::startFlash()
{
    m_flashing = true;
    updatePowerButtonStyle();
    m_flashTimer->start(kFlashDuration);
}

// ============================================================
// ⭐ 深蓝色时间到 → 变回浅蓝色，进入冷却
// ============================================================
void TitleBar::onFlashTimeout()
{
    m_flashing = false;
    updatePowerButtonStyle();
    m_cooldownTimer->start(kCooldownDuration);
}

// ============================================================
// ⭐ 冷却结束：如果有待处理的闪烁，立刻再闪一次
// ============================================================
void TitleBar::onCooldownTimeout()
{
    if (m_pendingFlash) {
        m_pendingFlash = false;
        startFlash();
    }
}

// ============================================================
// 设置标题
// ============================================================
void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

// ============================================================
// 开关状态
// ============================================================
bool TitleBar::isPowerOn() const
{
    return m_powerButton && m_powerButton->isChecked();
}

// ============================================================
// 点击串口按钮
// ============================================================
void TitleBar::onSerialButtonClicked()
{
    refreshSerialPorts();

    QPoint bottomLeft = m_serialButton->mapToGlobal(
        QPoint(0, m_serialButton->height() + 4));
    m_serialMenu->popup(bottomLeft);
}

// ============================================================
// 扫描串口
// ============================================================
void TitleBar::refreshSerialPorts()
{
    m_serialMenu->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    if (ports.isEmpty()) {
        QAction *emptyAct = m_serialMenu->addAction(QStringLiteral("无可用串口"));
        emptyAct->setEnabled(false);

        m_serialMenu->addSeparator();

        QAction *refreshAct = m_serialMenu->addAction(QStringLiteral("刷新"));
        connect(refreshAct, &QAction::triggered, this, [this]{
            onSerialButtonClicked();
        });
        return;
    }

    for (const QSerialPortInfo &info : ports) {
        QString label = info.portName();
        if (!info.description().isEmpty()) {
            label += "  ·  " + info.description();
        }

        QAction *act = m_serialMenu->addAction(label);
        act->setData(info.portName());
        act->setCheckable(true);
        act->setChecked(info.portName() == m_currentPort);

        connect(act, &QAction::triggered, this, [this, act]{
            m_currentPort = act->data().toString();
            m_serialButton->setText(m_currentPort);
            emit serialPortChanged(m_currentPort);
        });
    }

    m_serialMenu->addSeparator();

    QAction *refreshAct = m_serialMenu->addAction(QStringLiteral("刷新"));
    connect(refreshAct, &QAction::triggered, this, [this]{
        onSerialButtonClicked();
    });
}

// ============================================================
// 判断点击是否落在按钮上
// ============================================================
bool TitleBar::isOnAnyButton(const QPoint &pos) const
{
    QWidget *widgets[] = { m_titleLabel, m_powerButton, m_serialButton,
                          m_gearButton, m_minButton,   m_maxButton,
                          m_closeButton };
    for (QWidget *w : widgets) {
        if (w && w->isVisible() && w->geometry().contains(pos))
            return true;
    }
    return false;
}

// ============================================================
// 鼠标按下：拖窗口
// ============================================================
void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isOnAnyButton(event->pos())) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint()
                       - window()->frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        if (window()->isMaximized()) {
            qreal ratio = qreal(event->pos().x()) / qreal(width());
            window()->showNormal();
            QPoint newTL = event->globalPosition().toPoint()
                           - QPoint(int(window()->width() * ratio),
                                    m_dragOffset.y());
            window()->move(newTL);
            m_dragOffset = event->globalPosition().toPoint()
                           - window()->frameGeometry().topLeft();
        } else {
            window()->move(event->globalPosition().toPoint() - m_dragOffset);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isOnAnyButton(event->pos())) {
        emit maximizeClicked();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}