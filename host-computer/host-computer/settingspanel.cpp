#include "settingspanel.h"
#include <QAction>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QMenu(parent)
{
    setupUI();
}

void SettingsPanel::setupUI()
{
    setStyleSheet(
        "QMenu {"
        "    background-color: rgba(255, 255, 255, 245);"
        "    border: 1px solid rgba(0,0,0,30);"
        "    border-radius: 8px;"
        "    padding: 6px;"
        "    font-size: 13px;"
        "}"
        "QMenu::item {"
        "    padding: 8px 28px 8px 16px;"
        "    border-radius: 5px;"
        "    color: #333;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background: rgba(0,0,0,20);"
        "    margin: 4px 10px;"
        "}"
        );

    // ---- 菜单项 1：设置背景 ----
    QAction *actBg = addAction(QStringLiteral("设置背景"));
    connect(actBg, &QAction::triggered,
            this, &SettingsPanel::backgroundSettingsRequested);

    addSeparator();

    // ---- 菜单项 2：OTA升级（点一下直接发信号，弹出子窗口） ----
    QAction *actOta = addAction(QStringLiteral("OTA升级"));
    connect(actOta, &QAction::triggered,
            this, &SettingsPanel::otaUpgradeRequested);
}