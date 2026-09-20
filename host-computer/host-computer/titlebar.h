#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QPoint>
#include <QString>

class QLabel;
class QPushButton;
class QMenu;
class QTimer;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    bool isPowerOn() const;
    QString currentSerialPort() const { return m_currentPort; }

    QPushButton *gearButton()  const { return m_gearButton; }
    QPushButton *powerButton() const { return m_powerButton; }

    // ⭐ 外部调用：收到串口数据时触发闪烁
    void flashPowerButton();

signals:
    void powerToggled(bool checked);
    void gearClicked();
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void serialPortChanged(const QString &portName);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onSerialButtonClicked();
    void onFlashTimeout();                      // ⭐ 深蓝色 → 浅蓝色
    void onCooldownTimeout();                   // ⭐ 冷却结束

private:
    void setupUI();
    void refreshSerialPorts();
    bool isOnAnyButton(const QPoint &pos) const;

    // ⭐ 更新电源按钮样式（根据启动/闪烁状态）
    void updatePowerButtonStyle();
    void startFlash();                          // 开始一次深蓝色闪烁

    QLabel      *m_titleLabel   = nullptr;
    QPushButton *m_powerButton  = nullptr;
    QPushButton *m_serialButton = nullptr;
    QMenu       *m_serialMenu   = nullptr;
    QPushButton *m_gearButton   = nullptr;
    QPushButton *m_minButton    = nullptr;
    QPushButton *m_maxButton    = nullptr;
    QPushButton *m_closeButton  = nullptr;

    QString m_currentPort;

    // ⭐ 闪烁相关
    QTimer *m_flashTimer    = nullptr;   // 深蓝色持续时间
    QTimer *m_cooldownTimer = nullptr;   // 浅蓝色最小持续时间
    bool    m_flashing      = false;     // 当前是否处于深蓝色状态
    bool    m_pendingFlash  = false;     // 冷却期间是否有待处理的闪烁请求

    bool    m_dragging = false;
    QPoint  m_dragOffset;
};

#endif // TITLEBAR_H