#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QMenu>

class SettingsPanel : public QMenu
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

signals:
    void backgroundSettingsRequested();
    void otaUpgradeRequested();      // 点击 OTA升级 时发出

private:
    void setupUI();
};

#endif // SETTINGSPANEL_H