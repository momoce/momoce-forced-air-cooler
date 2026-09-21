#ifndef DEVICEPROMPTWIDGET_H
#define DEVICEPROMPTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QString>
#include <QPropertyAnimation>

class DevicePromptWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale WRITE setScale)

public:
    explicit DevicePromptWidget(QWidget *parent = nullptr);

    // 显示"找到设备"
    void setDeviceFound(const QString &portName);
    // 显示"未找到设备"
    void setDeviceNotFound();

    QString portName() const { return m_portName; }

    qreal scale() const { return m_scale; }
    void setScale(qreal s);

signals:
    // 用户点击正方形，动画结束后发出
    void connectRequested(const QString &portName);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
private:
    QRect cardRect() const;   // 展示设备的子窗口

    bool                m_found    = false;
    QString             m_portName;
    QPixmap             m_image;                 // 以后放设备图片
    qreal               m_scale    = 1.0;
    bool                m_animating = false;
    QPropertyAnimation *m_anim     = nullptr;
};

#endif // DEVICEPROMPTWIDGET_H