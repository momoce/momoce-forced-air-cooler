#ifndef CROPWIDGET_H
#define CROPWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPointF>

// 裁剪控件：滚轮缩放 + 按住拖动，控件本身即裁剪框
class CropWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CropWidget(const QPixmap &source, QWidget *parent = nullptr);

    QPixmap croppedImage() const;
    void    resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void clamp();

    QPixmap m_source;
    qreal   m_scale;
    QPointF m_offset;
    QPointF m_dragStart;
    QPointF m_offsetStart;
    bool    m_dragging;
    bool    m_initialized;
};

#endif // CROPWIDGET_H