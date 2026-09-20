#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include <QMenu>

// 设置菜单
class SettingsPanel : public QMenu
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

signals:
    void backgroundSettingsRequested();    // 用户点了"设置背景"

private:
    void setupUI();
};

#endif // SETTINGSPANEL_H