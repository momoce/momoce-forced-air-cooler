#ifndef OTAUPGRADEDIALOG_H
#define OTAUPGRADEDIALOG_H

#include <QDialog>
#include <QByteArray>
#include <QString>

class QSerialPort;
class QTcpSocket;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QComboBox;           // ★ 替代 QSpinBox
class QProgressBar;
class QPlainTextEdit;
class QTimer;

class OtaUpgradeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OtaUpgradeDialog(QSerialPort *serialPort, QWidget *parent = nullptr);
    ~OtaUpgradeDialog();

private slots:
    void onSelectFile();
    void onSendFirmware();
    void onRebootUpgrade();
    void onPollRxData();

private:
    void setupUI();
    void setUiBusy(bool busy);
    void appendLog(const QString &text);
    void appendRxLog(const QByteArray &data);
    bool isWifiMode() const;

    bool loadBinFile(const QString &path);
    bool openTransport();
    void closeTransport();
    qint64 writeBytes(const QByteArray &data);
    QByteArray readBytes(int timeoutMs);

    bool sendOtaStartCommand();
    bool waitOtaStartAck(int timeoutMs);
    bool waitOtaReady(int timeoutMs);
    bool sendFirmwareData();
    bool sendRebootCommand();
    bool waitRebootAck(int timeoutMs);

    QSerialPort    *m_serialPort = nullptr;
    QTcpSocket     *m_tcpSocket  = nullptr;

    QByteArray      m_fileData;
    QString         m_filePath;
    quint16         m_fileCrc = 0;

    bool            m_upgrading = false;

    QTimer         *m_rxTimer   = nullptr;

    // ---------- UI ----------
    QRadioButton   *m_rdoWired   = nullptr;
    QRadioButton   *m_rdoWifi    = nullptr;

    QLabel         *m_lblWifiIp    = nullptr;
    QLineEdit      *m_editWifiIp   = nullptr;
    QLabel         *m_lblWifiPort  = nullptr;
    QLineEdit      *m_editWifiPort = nullptr;

    QPushButton    *m_btnSelect  = nullptr;
    QLabel         *m_lblFile    = nullptr;
    QLabel         *m_lblSize    = nullptr;
    QLabel         *m_lblCrc     = nullptr;

    // ★ 传输参数：改为下拉菜单（不可手动输入）
    QComboBox      *m_comboChunkSize = nullptr;
    QComboBox      *m_comboInterval  = nullptr;

    QProgressBar   *m_progress   = nullptr;
    QLabel         *m_lblStatus  = nullptr;
    QPlainTextEdit *m_log        = nullptr;

    QPushButton    *m_btnClose        = nullptr;
    QPushButton    *m_btnReboot       = nullptr;
    QPushButton    *m_btnSendFirmware = nullptr;
};

#endif // OTAUPGRADEDIALOG_H