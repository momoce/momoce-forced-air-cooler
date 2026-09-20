#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QString>

class TitleBar;
class SettingsPanel;
class QSerialPort;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onGearClicked();
    void onOpenBackgroundDialog();
    void onPowerToggled(bool checked);
    void onSerialPortChanged(const QString &portName);
    void onSerialDataReceived();                       // ⭐ 收到串口数据

private:
    void setupUI();
    void updateLayout();

    void openSerialPort(const QString &portName);       // ⭐ 打开串口
    void closeSerialPort();                             // ⭐ 关闭串口

    QColor  m_backgroundColor;
    QPixmap m_backgroundImage;
    int     m_opacity;

    TitleBar      *m_titleBar     = nullptr;
    SettingsPanel *m_settingsMenu = nullptr;
    QSerialPort   *m_serialPort   = nullptr;            // ⭐ 串口对象
};

#endif // MAINWINDOW_H