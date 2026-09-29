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

// ============================================================
// 坐标转换
// ============================================================
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
// 绘制
// ============================================================
void PwmCurveChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // ---- 背景 ----
    p.fillRect(rect(), QColor(250, 250, 250));

    // ---- 标题 ----
    QFont titleFont = p.font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    p.setFont(titleFont);
    p.setPen(QColor(60, 60, 60));
    p.drawText(QRect(0, 4, width(), 22), Qt::AlignCenter, m_title);

    // ---- 绘图区 ----
    QRectF plotRect(m_leftMargin, m_topMargin,
                    width() - m_leftMargin - m_rightMargin,
                    height() - m_topMargin - m_bottomMargin);

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawRect(plotRect);

    // ---- 网格 ----
    p.setPen(QPen(QColor(220, 220, 220), 1, Qt::SolidLine));
    // 垂直网格（温度：10, 20, ... 100）
    for (int t = 10; t <= 100; t += 10) {
        double wx = toWidget(QPointF(t, 0)).x();
        p.drawLine(QPointF(wx, plotRect.top()),
                   QPointF(wx, plotRect.bottom()));
    }
    // 水平网格（PWM：0 ~ 7000，每 600 一条）
    for (int v = 0; v <= 7000; v += 600) {
        double wy = toWidget(QPointF(0, v)).y();
        p.drawLine(QPointF(plotRect.left(),  wy),
                   QPointF(plotRect.right(), wy));
    }

    // ---- 坐标轴边框 ----
    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.drawRect(plotRect);

    // ---- 坐标轴文字 ----
    p.setPen(QColor(80, 80, 80));
    QFont axisFont = p.font();
    axisFont.setBold(false);
    axisFont.setPointSize(8);
    p.setFont(axisFont);

    // X 轴（温度）
    for (int t = 10; t <= 100; t += 10) {
        double wx = toWidget(QPointF(t, 0)).x();
        p.drawText(QRectF(wx - 15, plotRect.bottom() + 2, 30, 15),
                   Qt::AlignCenter, QString::number(t));
    }
    // Y 轴（PWM）
    for (int v = 0; v <= 7000; v += 600) {
        double wy = toWidget(QPointF(0, v)).y();
        QString label = (v == 0) ? QStringLiteral("OFF") : QString::number(v);
        p.drawText(QRectF(0, wy - 8, m_leftMargin - 5, 16),
                   Qt::AlignRight | Qt::AlignVCenter, label);
    }

    if (m_data.isEmpty())
        return;

    // ---- 折线 ----
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(m_lineColor, 2));

    QPainterPath path;
    QPointF first = toWidget(m_data.first());
    path.moveTo(first);
    for (int i = 1; i < m_data.size(); ++i)
        path.lineTo(toWidget(m_data[i]));
    p.drawPath(path);

    // ---- 数据点 ----
    for (int i = 0; i < m_data.size(); ++i) {
        QPointF wp = toWidget(m_data[i]);

        // 点外圈
        p.setPen(QPen(m_lineColor, 1));
        p.setBrush(Qt::white);
        p.drawEllipse(wp, 5, 5);

        // 点内圈
        p.setPen(Qt::NoPen);
        p.setBrush(m_lineColor);
        p.drawEllipse(wp, 3, 3);
    }
}

// ============================================================
// 鼠标交互：拖动曲线点
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

    // X 轴限制在合理范围
    d.setX(qBound(m_xMin, d.x(), m_xMax));
    d.setY(qBound(m_yMin, d.y(), m_yMax));

    // 保持 X 轴递增（防止交叉）
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