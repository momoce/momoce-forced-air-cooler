#include "pwmcurvechart.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QtMath>

PwmCurveChart::PwmCurveChart(const QString &title,
                             const QColor &lineColor,
                             QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_lineColor(lineColor)
{
    setMinimumSize(300, 200);
    setMouseTracking(true);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    setStyleSheet("background: transparent;");
}

void PwmCurveChart::setData(const QVector<QPointF> &data)
{
    m_data = data;
    update();
}

void PwmCurveChart::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void PwmCurveChart::setXRange(double min, double max)
{
    m_xMin = min; m_xMax = max;
    update();
}

void PwmCurveChart::setYRange(double min, double max)
{
    m_yMin = min; m_yMax = max;
    update();
}

QPointF PwmCurveChart::toWidget(const QPointF &dataPoint) const
{
    double plotW = width()  - m_leftMargin - m_rightMargin;
    double plotH = height() - m_topMargin  - m_bottomMargin;

    double xRatio = (dataPoint.x() - m_xMin) / (m_xMax - m_xMin);
    double yRatio = (dataPoint.y() - m_yMin) / (m_yMax - m_yMin);

    double wx = m_leftMargin + xRatio * plotW;
    double wy = height() - m_bottomMargin - yRatio * plotH;
    return QPointF(wx, wy);
}

QPointF PwmCurveChart::toData(const QPointF &widgetPoint) const
{
    double plotW = width()  - m_leftMargin - m_rightMargin;
    double plotH = height() - m_topMargin  - m_bottomMargin;

    double xRatio = (widgetPoint.x() - m_leftMargin) / plotW;
    double yRatio = (height() - m_bottomMargin - widgetPoint.y()) / plotH;

    double dx = m_xMin + xRatio * (m_xMax - m_xMin);
    double dy = m_yMin + yRatio * (m_yMax - m_yMin);
    return QPointF(dx, dy);
}

int PwmCurveChart::hitTest(const QPointF &widgetPoint) const
{
    const double hitRadius = 10.0;
    for (int i = 0; i < m_data.size(); ++i) {
        QPointF wp = toWidget(m_data[i]);
        if (QLineF(wp, widgetPoint).length() <= hitRadius)
            return i;
    }
    return -1;
}

// ============================================================
// 绘制（全透明背景 + 灰白偏白文字）
// ============================================================
void PwmCurveChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ⭐ 不画任何实色背景，让主窗口背景图透出来

    // ---- 标题（灰白偏白 + 深色阴影） ----
    QFont titleFont = p.font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    p.setFont(titleFont);

    QRect titleRect(0, 4, width(), 22);
    p.setPen(QColor(0, 0, 0, 150));
    p.drawText(titleRect.translated(1, 1), Qt::AlignCenter, m_title);
    p.setPen(QColor(225, 225, 225));
    p.drawText(titleRect, Qt::AlignCenter, m_title);

    // ---- 绘图区 ----
    QRectF plotRect(m_leftMargin, m_topMargin,
                    width() - m_leftMargin - m_rightMargin,
                    height() - m_topMargin - m_bottomMargin);

    // ---- 网格线（半透明浅灰） ----
    p.setPen(QPen(QColor(200, 200, 200, 90), 1, Qt::SolidLine));

    for (int t = 10; t <= 100; t += 10) {
        double wx = toWidget(QPointF(t, 0)).x();
        p.drawLine(QPointF(wx, plotRect.top()),
                   QPointF(wx, plotRect.bottom()));
    }
    for (int v = 0; v <= 7000; v += 600) {
        double wy = toWidget(QPointF(0, v)).y();
        p.drawLine(QPointF(plotRect.left(),  wy),
                   QPointF(plotRect.right(), wy));
    }

    // ---- 坐标轴边框（半透明浅灰） ----
    p.setPen(QPen(QColor(200, 200, 200, 150), 1));
    p.drawRect(plotRect);

    // ---- 坐标轴文字（灰白偏白 + 深色阴影） ----
    QFont axisFont = p.font();
    axisFont.setBold(false);
    axisFont.setPointSize(8);
    p.setFont(axisFont);

    auto drawLabel = [&](const QRectF &r, const QString &text, int align) {
        p.setPen(QColor(0, 0, 0, 150));
        p.drawText(r.translated(1, 1), align, text);
        p.setPen(QColor(215, 215, 215));
        p.drawText(r, align, text);
    };

    // X 轴（温度）
    for (int t = 10; t <= 100; t += 10) {
        double wx = toWidget(QPointF(t, 0)).x();
        drawLabel(QRectF(wx - 15, plotRect.bottom() + 2, 30, 15),
                  QString::number(t), Qt::AlignCenter);
    }
    // Y 轴（PWM）
    for (int v = 0; v <= 7000; v += 600) {
        double wy = toWidget(QPointF(0, v)).y();
        QString label = (v == 0) ? QStringLiteral("OFF") : QString::number(v);
        drawLabel(QRectF(0, wy - 8, m_leftMargin - 5, 16),
                  label, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (m_data.isEmpty())
        return;

    // ---- 折线 ----
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    QPointF first = toWidget(m_data.first());
    path.moveTo(first);
    for (int i = 1; i < m_data.size(); ++i)
        path.lineTo(toWidget(m_data[i]));

    // 深色底描边（浅色背景上也能看清）
    p.setPen(QPen(QColor(0, 0, 0, 120), 4));
    p.drawPath(path);

    // 彩色主线
    p.setPen(QPen(m_lineColor, 2));
    p.drawPath(path);

    // ---- 数据点 ----
    for (int i = 0; i < m_data.size(); ++i) {
        QPointF wp = toWidget(m_data[i]);

        // 深色外圈
        p.setPen(QPen(QColor(0, 0, 0, 120), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(wp, 5, 5);

        // 彩色内芯
        p.setPen(QPen(m_lineColor, 1));
        p.setBrush(m_lineColor);
        p.drawEllipse(wp, 3, 3);
    }
}

// ============================================================
// 鼠标交互
// ============================================================
void PwmCurveChart::mousePressEvent(QMouseEvent *event)
{
    m_draggingIndex = hitTest(event->position());
    if (m_draggingIndex >= 0)
        setCursor(Qt::ClosedHandCursor);
}

void PwmCurveChart::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingIndex < 0) {
        int idx = hitTest(event->position());
        setCursor(idx >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }

    QPointF d = toData(event->position());

    d.setX(qBound(m_xMin, d.x(), m_xMax));
    d.setY(qBound(m_yMin, d.y(), m_yMax));

    if (m_draggingIndex > 0)
        d.setX(qMax(d.x(), m_data[m_draggingIndex - 1].x() + 1.0));
    if (m_draggingIndex < m_data.size() - 1)
        d.setX(qMin(d.x(), m_data[m_draggingIndex + 1].x() - 1.0));

    m_data[m_draggingIndex] = d;
    emit pointMoved(m_draggingIndex, d);
    update();
}

void PwmCurveChart::mouseReleaseEvent(QMouseEvent *)
{
    m_draggingIndex = -1;
    setCursor(Qt::ArrowCursor);
}