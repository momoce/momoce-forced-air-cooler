#include "devicepromptwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFont>
#include <QDebug>

// 卡片宽、高分别占 widget 宽、高的比例
static const qreal kCardWidthRatio  = 0.42;
static const qreal kCardHeightRatio = 0.42;

// 图片路径（资源文件里注册）
static const char *kDeviceImagePath    = ":/image/momoce.png";    // 找到设备
static const char *kNotFoundImagePath  = ":/image/not_found.png"; // 未找到设备（可选）

DevicePromptWidget::DevicePromptWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_anim = new QPropertyAnimation(this, "scale", this);
    m_anim->setDuration(180);
    m_anim->setStartValue(1.0);
    m_anim->setKeyValueAt(0.5, 0.9);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_anim, &QPropertyAnimation::finished, this, [this]{
        m_animating = false;
        emit connectRequested(m_portName);
        hide();
    });
}

void DevicePromptWidget::setDeviceFound(const QString &portName)
{
    m_found    = true;
    m_portName = portName;

    m_image.load(kDeviceImagePath);
    if (m_image.isNull())
        qDebug() << "设备图片加载失败：" << kDeviceImagePath;
    else
        qDebug() << "设备图片加载成功，大小：" << m_image.size();

    update();
    show();
    raise();
}

void DevicePromptWidget::setDeviceNotFound()
{
    m_found = false;
    m_portName.clear();

    m_image.load(kNotFoundImagePath);
    if (m_image.isNull())
        m_image = QPixmap();   // 没图片就空白卡片

    update();
    show();
    raise();
}

void DevicePromptWidget::setScale(qreal s)
{
    m_scale = s;
    update();
}

QRect DevicePromptWidget::cardRect() const
{
    int w = int(width()  * kCardWidthRatio  * m_scale);
    int h = int(height() * kCardHeightRatio * m_scale);

    w = qMax(w, 80);
    h = qMax(h, 80);

    int x = (width()  - w) / 2;
    int y = (height() - h) / 2;
    return QRect(x, y, w, h);
}

void DevicePromptWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // 1. 半透明遮罩
    p.fillRect(rect(), QColor(0, 0, 0, 130));

    // 2. 圆角卡片
    const QRect card = cardRect();
    const int   radius = qMin(card.width(), card.height()) / 8;

    QPainterPath path;
    path.addRoundedRect(QRectF(card), radius, radius);

    p.fillPath(path, QColor(255, 255, 255, 245));
    p.setPen(QPen(QColor(0, 0, 0, 50), 1));
    p.drawPath(path);

    // 3. 只画图片，不画任何文字
    if (!m_image.isNull()) {
        // 图片尽量占满卡片，只留一点点内边距
        QRect imgRect = card.adjusted(12, 12, -12, -12);

        QPixmap scaled = m_image.scaled(
            imgRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        int x = imgRect.x() + (imgRect.width()  - scaled.width())  / 2;
        int y = imgRect.y() + (imgRect.height() - scaled.height()) / 2;
        p.drawPixmap(x, y, scaled);
    }
    // 没有图片时，卡片就是空白白色圆角矩形
}

void DevicePromptWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_animating) {
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton
        && m_found
        && cardRect().contains(event->pos())) {
        m_animating = true;
        m_anim->start();
        event->accept();
        return;
    }

    event->accept();
}

void DevicePromptWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}