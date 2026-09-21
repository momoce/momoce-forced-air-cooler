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
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int kFlashDuration    = 50;
static const int kCooldownDuration = 50;

static const QString kColorOff        = "#cccccc";
static const QString kColorOffHover   = "#bfbfbf";
static const QString kColorOffBorder  = "#b0b0b0";

static const QString kColorOn         = "#64B5F6";
static const QString kColorOnHover    = "#42A5F5";
static const QString kColorOnBorder   = "#42A5F5";

static const QString kColorFlash      = "#1976D2";
static const QString kColorFlashHover = "#1565C0";
static const QString kColorFlashBorder= "#0D47A1";

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

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("customTitleBar");
    setAttribute(Qt::WA_StyledBackground, false);

    setStyleSheet(
        "#customTitleBar {"
        "    background-color: transparent;"
        "    border: none;"
        "}");

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

void TitleBar::setupUI()
{
    m_titleLabel = new QLabel(QStringLiteral("压风式散热器"), this);
    m_titleLabel->setStyleSheet(
        "QLabel { color: #333; font-size: 14px; font-weight: bold; }");

    m_powerButton = new QPushButton(this);
    m_powerButton->setFixedSize(28, 28);
    m_powerButton->setCheckable(true);
    m_powerButton->setChecked(false);
    m_powerButton->setCursor(Qt::PointingHandCursor);
    m_powerButton->setToolTip(QStringLiteral("启动 / 停止"));
    m_powerButton->setToolTipDuration(2000);

    updatePowerButtonStyle();

    connect(m_powerButton, &QPushButton::toggled, this, [this](bool checked){
        if (!checked) {
            m_flashing     = false;
            m_pendingFlash = false;
            m_flashTimer->stop();
            m_cooldownTimer->stop();
        }
        updatePowerButtonStyle();
    });

    connect(m_powerButton, &QPushButton::toggled,
            this, &TitleBar::powerToggled);

    m_presets = QStringList{ QStringLiteral("默认预设"), QStringLiteral("预设 2") };
    m_currentPreset = m_presets.first();

    m_presetButton = new QPushButton(this);
    m_presetButton->setFixedHeight(28);
    m_presetButton->setMinimumWidth(130);
    m_presetButton->setCursor(Qt::PointingHandCursor);
    m_presetButton->setText(m_currentPreset);
    m_presetButton->setToolTip(QStringLiteral("选择预设"));
    m_presetButton->setStyleSheet(
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

    m_addPresetButton = new QPushButton(QStringLiteral("+"), this);
    m_addPresetButton->setFixedSize(28, 28);
    m_addPresetButton->setCursor(Qt::PointingHandCursor);
    m_addPresetButton->setToolTip(QStringLiteral("新建预设"));
    m_addPresetButton->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 200);"
        "    border: 1px solid rgba(0, 0, 0, 40);"
        "    border-radius: 6px;"
        "    font-size: 16px; font-weight: bold;"
        "    color: #333; padding: 0;"
        "}"
        "QPushButton:hover   { background-color: rgba(255, 255, 255, 240); }"
        "QPushButton:pressed { background-color: rgba(220, 220, 220, 240); }");

    m_removePresetButton = new QPushButton(QStringLiteral("-"), this);
    m_removePresetButton->setFixedSize(28, 28);
    m_removePresetButton->setCursor(Qt::PointingHandCursor);
    m_removePresetButton->setToolTip(QStringLiteral("删除预设"));
    m_removePresetButton->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 200);"
        "    border: 1px solid rgba(0, 0, 0, 40);"
        "    border-radius: 6px;"
        "    font-size: 16px; font-weight: bold;"
        "    color: #333; padding: 0;"
        "}"
        "QPushButton:hover   { background-color: rgba(255, 255, 255, 240); }"
        "QPushButton:pressed { background-color: rgba(220, 220, 220, 240); }");

    m_renameButton = new QPushButton(QStringLiteral("重命名"), this);
    m_renameButton->setFixedHeight(28);
    m_renameButton->setMinimumWidth(64);
    m_renameButton->setCursor(Qt::PointingHandCursor);
    m_renameButton->setToolTip(QStringLiteral("重命名当前预设"));
    m_renameButton->setStyleSheet(
        "QPushButton {"
        "    background-color: rgba(255, 255, 255, 200);"
        "    border: 1px solid rgba(0, 0, 0, 40);"
        "    border-radius: 6px;"
        "    padding: 0 10px;"
        "    font-size: 12px; color: #333;"
        "}"
        "QPushButton:hover   { background-color: rgba(255, 255, 255, 240); }"
        "QPushButton:pressed { background-color: rgba(220, 220, 220, 240); }");

    m_presetMenu = new QMenu(this);
    m_presetMenu->setStyleSheet(
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

    connect(m_presetButton,       &QPushButton::clicked, this, &TitleBar::onPresetButtonClicked);
    connect(m_addPresetButton,    &QPushButton::clicked, this, &TitleBar::onAddPresetClicked);
    connect(m_removePresetButton, &QPushButton::clicked, this, &TitleBar::onRemovePresetClicked);
    connect(m_renameButton,       &QPushButton::clicked, this, &TitleBar::onRenamePresetClicked);

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

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 8, 0);
    layout->setSpacing(8);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_powerButton);
    layout->addWidget(m_presetButton);
    layout->addWidget(m_addPresetButton);
    layout->addWidget(m_removePresetButton);
    layout->addWidget(m_renameButton);
    layout->addStretch();
    layout->addWidget(m_gearButton);
    layout->addWidget(m_minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(m_closeButton);
}

void TitleBar::refreshPresetMenu()
{
    m_presetMenu->clear();

    if (m_presets.isEmpty()) {
        QAction *emptyAct = m_presetMenu->addAction(QStringLiteral("暂无预设"));
        emptyAct->setEnabled(false);
        return;
    }

    for (const QString &name : m_presets) {
        QAction *act = m_presetMenu->addAction(name);
        act->setCheckable(true);
        act->setChecked(name == m_currentPreset);
        act->setData(name);

        connect(act, &QAction::triggered, this, [this, name]{
            m_currentPreset = name;
            m_presetButton->setText(name);
            emit presetChanged(name);
        });
    }
}

void TitleBar::onPresetButtonClicked()
{
    refreshPresetMenu();

    QPoint bottomLeft = m_presetButton->mapToGlobal(
        QPoint(0, m_presetButton->height() + 4));
    m_presetMenu->popup(bottomLeft);
}

void TitleBar::onAddPresetClicked()
{
    bool ok = false;
    QString name = QInputDialog::getText(
        this,
        QStringLiteral("新建预设"),
        QStringLiteral("请输入预设名称："),
        QLineEdit::Normal,
        QStringLiteral("新预设"),
        &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    name = name.trimmed();

    if (m_presets.contains(name)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已存在同名预设。"));
        return;
    }

    m_presets.append(name);
    m_currentPreset = name;
    m_presetButton->setText(name);

    emit presetAdded(name);
    emit presetChanged(name);
}

void TitleBar::onRemovePresetClicked()
{
    if (m_presets.isEmpty())
        return;

    if (m_currentPreset.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个预设。"));
        return;
    }

    QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        QStringLiteral("删除预设"),
        QStringLiteral("确定删除预设 \"%1\" 吗？").arg(m_currentPreset),
        QMessageBox::Yes | QMessageBox::No);

    if (ret != QMessageBox::Yes)
        return;

    QString removed = m_currentPreset;
    m_presets.removeAll(removed);

    m_currentPreset = m_presets.isEmpty() ? QString() : m_presets.first();
    m_presetButton->setText(m_currentPreset.isEmpty()
                                ? QStringLiteral("选择预设")
                                : m_currentPreset);

    emit presetRemoved(removed);
    emit presetChanged(m_currentPreset);
}

void TitleBar::onRenamePresetClicked()
{
    if (m_currentPreset.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个预设。"));
        return;
    }

    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        QStringLiteral("重命名预设"),
        QStringLiteral("请输入新的名称："),
        QLineEdit::Normal,
        m_currentPreset,
        &ok);

    if (!ok)
        return;

    newName = newName.trimmed();
    if (newName.isEmpty() || newName == m_currentPreset)
        return;

    if (m_presets.contains(newName)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已存在同名预设。"));
        return;
    }

    QString oldName = m_currentPreset;
    int idx = m_presets.indexOf(oldName);
    if (idx >= 0)
        m_presets[idx] = newName;

    m_currentPreset = newName;
    m_presetButton->setText(newName);

    emit presetRenamed(oldName, newName);
    emit presetChanged(newName);
}

void TitleBar::updatePowerButtonStyle()
{
    bool on = m_powerButton->isChecked();

    QString bg, hoverBg, border;
    if (!on) {
        bg      = kColorOff;
        hoverBg = kColorOffHover;
        border  = kColorOffBorder;
    } else if (m_flashing) {
        bg      = kColorFlash;
        hoverBg = kColorFlashHover;
        border  = kColorFlashBorder;
    } else {
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

void TitleBar::flashPowerButton()
{
    if (!m_powerButton->isChecked()) return;
    if (m_flashing) return;

    if (m_cooldownTimer->isActive()) {
        m_pendingFlash = true;
        return;
    }

    startFlash();
}

void TitleBar::startFlash()
{
    m_flashing = true;
    updatePowerButtonStyle();
    m_flashTimer->start(kFlashDuration);
}

void TitleBar::onFlashTimeout()
{
    m_flashing = false;
    updatePowerButtonStyle();
    m_cooldownTimer->start(kCooldownDuration);
}

void TitleBar::onCooldownTimeout()
{
    if (m_pendingFlash) {
        m_pendingFlash = false;
        startFlash();
    }
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

bool TitleBar::isPowerOn() const
{
    return m_powerButton && m_powerButton->isChecked();
}

bool TitleBar::isOnAnyButton(const QPoint &pos) const
{
    QWidget *widgets[] = {
        m_titleLabel, m_powerButton,
        m_presetButton, m_addPresetButton, m_removePresetButton, m_renameButton,
        m_gearButton, m_minButton, m_maxButton, m_closeButton
    };
    for (QWidget *w : widgets) {
        if (w && w->isVisible() && w->geometry().contains(pos))
            return true;
    }
    return false;
}

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