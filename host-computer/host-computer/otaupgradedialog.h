#ifndef OTAUPGRADEDIALOG_H
#define OTAUPGRADEDIALOG_H

#include <QDialog>
#include <QByteArray>
#include <QString>

class QSerialPort;
class QTcpSocket;
class QTimer;

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QProgressBar;
class QPlainTextEdit;

class OtaUpgradeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OtaUpgradeDialog(QSerialPort *serialPort, QWidget *parent = nullptr);
    ~OtaUpgradeDialog() override;

private slots:
    void onSelectFile();
    void onSendFirmware();     // 原 onStartUpgrade
    void onRebootUpgrade();
    void onPollRxData();

private:
    void setupUI();
    void setUiBusy(bool busy);
    void appendLog(const QString &text);
    void appendRxLog(const QByteArray &data);

    bool isWifiMode() const;
    bool loadBinFile(const QString &path);

    // 底层传输
    bool       openTransport();
    void       closeTransport();
    qint64     writeBytes(const QByteArray &data);
    QByteArray readBytes(int timeoutMs);

    // 协议
    bool sendOtaStartCommand();
    bool waitOtaStartAck(int timeoutMs);
    bool waitOtaReady(int timeoutMs);      // ★ 新增：等待 OTA_READY
    bool sendFirmwareData();
    bool sendRebootCommand();
    bool waitRebootAck(int timeoutMs);

private:
    QSerialPort *m_serialPort = nullptr;
    QTcpSocket  *m_tcpSocket  = nullptr;
    QTimer      *m_rxTimer    = nullptr;

    bool        m_upgrading   = false;
    QByteArray  m_fileData;
    QString     m_filePath;
    quint16     m_fileCrc     = 0;

    // 升级方式
    QRadioButton *m_rdoWired = nullptr;
    QRadioButton *m_rdoWifi  = nullptr;

    // WiFi 参数
    QLabel    *m_lblWifiIp    = nullptr;
    QLineEdit *m_editWifiIp   = nullptr;
    QLabel    *m_lblWifiPort  = nullptr;
    QLineEdit *m_editWifiPort = nullptr;

    // 文件
    QPushButton *m_btnSelect = nullptr;
    QLabel      *m_lblFile   = nullptr;
    QLabel      *m_lblSize   = nullptr;
    QLabel      *m_lblCrc    = nullptr;

    // 传输参数
    QSpinBox *m_spinChunkSize  = nullptr;
    QSpinBox *m_spinIntervalMs = nullptr;

    // 进度 / 状态
    QProgressBar    *m_progress = nullptr;
    QLabel          *m_lblStatus = nullptr;
    QPlainTextEdit  *m_log       = nullptr;

    // 按钮
    QPushButton *m_btnClose        = nullptr;
    QPushButton *m_btnReboot       = nullptr;
    QPushButton *m_btnSendFirmware = nullptr;
};

#endif // OTAUPGRADEDIALOG_H