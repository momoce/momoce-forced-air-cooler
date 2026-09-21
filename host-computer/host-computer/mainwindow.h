#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include "modbus_rtu.h"

class TitleBar;
class SettingsPanel;
class QSerialPort;
class QMessageBox;
class QTimer;
class QShowEvent;
class QResizeEvent;
class QPaintEvent;
class DevicePromptWidget;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onGearClicked();
    void onOpenBackgroundDialog();
    void onPowerToggled(bool checked);
    void onSerialDataReceived();

    void onDeviceFound(const QString &portName);
    void onScanFinished();
    void checkForNewPorts();

    void onConnectRequested(const QString &portName);
    void onConnectTimeout();
    void onDisconnectTimeout();

    void onHeartbeatTimeout();

private:
    void setupUI();
    void updateLayout();

    void openSerialPort(const QString &portName);
    void closeSerialPort();

    void startConnectHandshake(const QString &portName);

    void startHeartbeat();
    void stopHeartbeat();
    void onHeartbeatLost();

    void        startScan(const QStringList &ports);
    QStringList allAvailablePorts() const;
    bool        isPortAvailable(const QString &portName) const;

    // 自动扫描设备相关状态
    bool         m_scanInProgress    = false;
    bool         m_deviceFound       = false;
    bool         m_deviceConnected   = false;
    QString      m_foundPortName;
    QString      m_currentPortName;
    QStringList  m_knownPorts;
    QTimer      *m_portCheckTimer    = nullptr;

    // 连接 / 断开握手
    bool         m_connectPending    = false;
    bool         m_disconnectPending = false;
    QTimer      *m_connectTimeout    = nullptr;
    QTimer      *m_disconnectTimeout = nullptr;
    QByteArray   m_rxBuffer;

    // 心跳
    QTimer      *m_heartbeatTimer            = nullptr;
    int          m_heartbeatMissCount        = 0;
    bool         m_heartbeatResponseReceived = false;
    bool         m_heartbeatFirst            = true;

    // 界面
    QColor  m_backgroundColor;
    QPixmap m_backgroundImage;
    int     m_opacity = 255;

    TitleBar           *m_titleBar     = nullptr;
    SettingsPanel      *m_settingsMenu = nullptr;
    QSerialPort        *m_serialPort   = nullptr;
    ModbusScanner      *m_scanner      = nullptr;
    DevicePromptWidget *m_devicePrompt = nullptr;

    QString m_currentPreset;
};

#endif // MAINWINDOW_H