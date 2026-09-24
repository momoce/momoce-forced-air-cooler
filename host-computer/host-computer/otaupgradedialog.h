#ifndef OTAUPGRADEDIALOG_H
#define OTAUPGRADEDIALOG_H

#include <QDialog>
#include <QString>
#include <QByteArray>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QProgressBar;
class QSpinBox;
class QLineEdit;
class QRadioButton;
class QSerialPort;
class QTcpSocket;

class OtaUpgradeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OtaUpgradeDialog(QSerialPort *serialPort,
                              QWidget *parent = nullptr);
    ~OtaUpgradeDialog() override;

private slots:
    void onSelectFile();
    void onStartUpgrade();

private:
    void setupUI();
    void setUiBusy(bool busy);
    void appendLog(const QString &text);

    bool loadBinFile(const QString &path);

    bool sendOtaStartCommand();
    bool waitOtaStartAck(int timeoutMs = 1500);
    bool sendFirmwareData();
    bool sendOtaDoneCommand();
    bool waitOtaDoneAck(int timeoutMs = 3000);

    bool       openTransport();
    void       closeTransport();
    qint64     writeBytes(const QByteArray &data);
    QByteArray readBytes(int timeoutMs);
    bool       isWifiMode() const;

private:
    QSerialPort    *m_serialPort = nullptr;
    QTcpSocket     *m_tcpSocket  = nullptr;

    // ---- 升级方式 ----
    QRadioButton   *m_rdoWired   = nullptr;
    QRadioButton   *m_rdoWifi    = nullptr;
    QLabel         *m_lblWifiIp  = nullptr;
    QLineEdit      *m_editWifiIp = nullptr;
    QLabel         *m_lblWifiPort  = nullptr;
    QLineEdit      *m_editWifiPort = nullptr;

    // ---- 传输参数 ----
    QSpinBox       *m_spinChunkSize  = nullptr;   // 每包字节数
    QSpinBox       *m_spinIntervalMs = nullptr;   // 包间隔毫秒

    // ---- 文件 ----
    QLabel         *m_lblFile    = nullptr;
    QLabel         *m_lblSize    = nullptr;
    QLabel         *m_lblCrc     = nullptr;
    QLabel         *m_lblStatus  = nullptr;
    QProgressBar   *m_progress   = nullptr;
    QPlainTextEdit *m_log        = nullptr;

    QPushButton    *m_btnSelect  = nullptr;
    QPushButton    *m_btnStart   = nullptr;
    QPushButton    *m_btnClose   = nullptr;

    QString    m_filePath;
    QByteArray m_fileData;
    quint16    m_fileCrc = 0;
    bool       m_upgrading = false;
};

#endif // OTAUPGRADEDIALOG_H