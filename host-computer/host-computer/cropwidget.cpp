#include "cropwidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QtMath>

CropWidget::CropWidget(const QPixmap &source, QWidget *parent)
    : QWidget(parent)
    , m_source(source)
    , m_scale(1.0)
    , m_dragging(false)
    , m_initialized(false)
{
    setCursor(Qt::OpenHandCursor);
    setMinimumSize(200, 120);
}

// ============================================================
// 初始化：让图片覆盖整个裁剪区并居中
// ============================================================
void CropWidget::resetView()
{
    if (m_source.isNull() || width() <= 0 || height() <= 0) return;

    qreal sx = qreal(width())  / m_source.width();
    qreal sy = qreal(height()) / m_source.height();
    m_scale = qMax(sx, sy);

    qreal w = m_source.width()  * m_scale;
    qreal h = m_source.height() * m_scale;
    m_offset = QPointF((width() - w) / 2.0, (height() - h) / 2.0);

    update();
}

// ============================================================
// 限制图片不露出裁剪区
// ============================================================
void CropWidget::clamp()
{
    qreal w = m_source.width()  * m_scale;
    qreal h = m_source.height() * m_scale;

    if (w <= width()) {
        m_offset.setX((width() - w) / 2.0);
    } else {
        if (m_offset.x() > 0)           m_offset.setX(0);
        if (m_offset.x() + w < width()) m_offset.setX(width() - w);
    }

    if (h <= height()) {
        m_offset.setY((height() - h) / 2.0);
    } else {
        if (m_offset.y() > 0)            m_offset.setY(0);
        if (m_offset.y() + h < height()) m_offset.setY(height() - h);
    }
}

// ============================================================
// 绘制
// ============================================================
void CropWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.fillRect(rect(), QColor(30, 30, 30));

    if (m_source.isNull()) return;

    QRectF target(m_offset,
                  QSizeF(m_source.width()  * m_scale,
                         m_source.height() * m_scale));
    p.drawPixmap(target, m_source, QRectF(m_source.rect()));

    p.setPen(QPen(QColor(220, 220, 220, 150), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ============================================================
// 滚轮缩放
// ============================================================
void CropWidget::wheelEvent(QWheelEvent *e)
{
    if (m_source.isNull()) return;

    QPointF mousePos = e->position();
    QPointF imgCoord = (mousePos - m_offset) / m_scale;

    qreal factor = (e->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    qreal newScale = m_scale * factor;

    qreal minScale = qMax(qreal(width())  / m_source.width(),
                          qreal(height()) / m_source.height());
    if (newScale < minScale) newScale = minScale;
    if (newScale > 10.0)     newScale = 10.0;

    m_scale = newScale;
    m_offset = mousePos - imgCoord * m_scale;

    clamp();
    update();
}

// ============================================================
// 鼠标按下
// ============================================================
void CropWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging    = true;
        m_dragStart   = e->position();
        m_offsetStart = m_offset;
        setCursor(Qt::ClosedHandCursor);
    }
}

// ============================================================
// 鼠标移动
// ============================================================
void CropWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    QPointF delta = e->position() - m_dragStart;
    m_offset = m_offsetStart + delta;
    clamp();
    update();
}

// ============================================================
// 鼠标释放
// ============================================================
void CropWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

// ============================================================
// 尺寸变化
// ============================================================
void CropWidget::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_source.isNull()) return;

    if (!m_initialized) {
        m_initialized = true;
        resetView();
    } else {
        clamp();
        update();
    }
}

// ============================================================
// 裁剪结果
// ============================================================
QPixmap CropWidget::croppedImage() const
{
    if (m_source.isNull()) return QPixmap();

    QRectF srcRect(
        -m_offset.x() / m_scale,
        -m_offset.y() / m_scale,
        width()       / m_scale,
        height()      / m_scale
        );
    QRect srcInt = srcRect.toRect().intersected(m_source.rect());
    return m_source.copy(srcInt);
}