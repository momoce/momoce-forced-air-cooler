#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QStringList>

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

    QString currentPreset() const { return m_currentPreset; }

    QPushButton *gearButton()  const { return m_gearButton; }
    QPushButton *powerButton() const { return m_powerButton; }

    void flashPowerButton();

signals:
    void powerToggled(bool checked);
    void gearClicked();
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

    void presetChanged(const QString &name);
    void presetAdded(const QString &name);
    void presetRemoved(const QString &name);
    void presetRenamed(const QString &oldName, const QString &newName);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onPresetButtonClicked();
    void onAddPresetClicked();
    void onRemovePresetClicked();
    void onRenamePresetClicked();

    void onFlashTimeout();
    void onCooldownTimeout();

private:
    void setupUI();
    bool isOnAnyButton(const QPoint &pos) const;

    void updatePowerButtonStyle();
    void startFlash();

    void refreshPresetMenu();

    QLabel      *m_titleLabel   = nullptr;
    QPushButton *m_powerButton  = nullptr;
    QPushButton *m_gearButton   = nullptr;
    QPushButton *m_minButton    = nullptr;
    QPushButton *m_maxButton    = nullptr;
    QPushButton *m_closeButton  = nullptr;

    QPushButton *m_presetButton       = nullptr;
    QPushButton *m_addPresetButton    = nullptr;
    QPushButton *m_removePresetButton = nullptr;
    QPushButton *m_renameButton       = nullptr;
    QMenu       *m_presetMenu         = nullptr;

    QStringList m_presets;
    QString     m_currentPreset;

    QTimer *m_flashTimer    = nullptr;
    QTimer *m_cooldownTimer = nullptr;
    bool    m_flashing      = false;
    bool    m_pendingFlash  = false;

    bool    m_dragging = false;
    QPoint  m_dragOffset;
};

#endif // TITLEBAR_H