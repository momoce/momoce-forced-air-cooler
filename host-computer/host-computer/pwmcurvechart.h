#ifndef PWMCURVECHART_H
#define PWMCURVECHART_H

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>

class PwmCurveChart : public QWidget
{
    Q_OBJECT
public:
    explicit PwmCurveChart(const QString &title,
                           const QColor &lineColor,
                           QWidget *parent = nullptr);

    void setData(const QVector<QPointF> &data);
    QVector<QPointF> data() const { return m_data; }

    void setTitle(const QString &title);

    void setXRange(double min, double max);
    void setYRange(double min, double max);

signals:
    void pointMoved(int index, QPointF newPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPointF toWidget(const QPointF &dataPoint) const;
    QPointF toData(const QPointF &widgetPoint) const;
    int     hitTest(const QPointF &widgetPoint) const;

    QString          m_title;
    QColor           m_lineColor;
    QVector<QPointF> m_data;

    double m_xMin = 10.0;
    double m_xMax = 100.0;
    double m_yMin = 0.0;
    double m_yMax = 7000.0;

    int m_draggingIndex = -1;

    const int m_leftMargin   = 55;
    const int m_rightMargin  = 20;
    const int m_topMargin    = 30;
    const int m_bottomMargin = 35;
};

#endif // PWMCURVECHART_H