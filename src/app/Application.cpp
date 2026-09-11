#include "app/Application.h"
#include "app/ConfigStore.h"
#include "platform/AutoStart.h"
#include "platform/GlobalHotkey.h"
#include "ui/FloatWindow.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QHash>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {
    setQuitOnLastWindowClosed(false);
    m_config = ConfigStore::normalize(ConfigStore::load());
    m_iconChoice = m_config.value(QStringLiteral("app_icon")).toString(QStringLiteral("default"));

    m_window = new FloatWindow(m_config);
    m_window->setOpenSettingsCallback([this] { openSettings(); });
    connect(m_window, &FloatWindow::configChanged, this, &Application::saveConfig);

    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::activated, this, &Application::toggleWindow);
    m_appliedHotkey = m_config.value(QStringLiteral("hotkey")).toString(QStringLiteral("Ctrl+Alt+F"));
    applyHotkey(m_appliedHotkey);

    m_appliedStartOnBoot = m_config.value(QStringLiteral("start_on_boot")).toBool(false);
    AutoStart::setEnabled(m_appliedStartOnBoot);
    applyIcon(m_iconChoice);

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(QStringLiteral("StockWidget"));
    auto* menu = new QMenu();
    menu->addAction(QStringLiteral("显示/隐藏 浮窗"), this, &Application::toggleWindow);
    menu->addAction(QStringLiteral("定位浮窗"), this, &Application::locateWindow);
    menu->addAction(QStringLiteral("设置…"), this, &Application::openSettings);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &Application::quitApp);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
                if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
                    toggleWindow();
            });
    m_tray->show();

    m_window->show();
    m_window->raise();
    m_window->activateWindow();
    saveConfig();
}

QIcon Application::resolveIcon(const QString& choice) const {
    if (choice.isEmpty() || choice == QStringLiteral("default"))
        return QIcon(QStringLiteral(":/StockWidget.ico"));
    if (choice.startsWith(QStringLiteral("std:"))) {
        const QString key = choice.mid(4);
        static const QHash<QString, QStyle::StandardPixmap> map{
            {QStringLiteral("computer"), QStyle::SP_ComputerIcon},
            {QStringLiteral("network"), QStyle::SP_DriveNetIcon},
            {QStringLiteral("folder"), QStyle::SP_DirIcon},
            {QStringLiteral("file"), QStyle::SP_FileIcon},
            {QStringLiteral("trash"), QStyle::SP_TrashIcon},
            {QStringLiteral("desktop"), QStyle::SP_DesktopIcon}};
        return style()->standardIcon(map.value(key, QStyle::SP_ComputerIcon));
    }
    return QIcon(choice);
}

void Application::applyIcon(const QString& choice) {
    m_iconChoice = choice;
    const QIcon icon = resolveIcon(choice);
    setWindowIcon(icon);
    if (m_tray) m_tray->setIcon(icon);
}

void Application::applyHotkey(const QString& sequence) {
    if (m_hotkey) m_hotkey->registerShortcut(GlobalHotkey::normalize(sequence));
}

void Application::toggleWindow() {
    if (m_window->isVisible()) {
        m_window->hide();
    } else {
        m_window->show();
        m_window->raise();
        m_window->activateWindow();
    }
    saveConfig();
}

void Application::locateWindow() {
    if (m_window) m_window->locate();
}

void Application::openSettings() { SettingsDialog::showFor(m_window, m_window); }

void Application::saveConfig() {
    if (!m_window) return;
    QJsonObject cfg = ConfigStore::normalize(m_window->currentConfig());

    const QString hotkey = cfg.value(QStringLiteral("hotkey")).toString(QStringLiteral("Ctrl+Alt+F"));
    if (hotkey != m_appliedHotkey) {
        m_appliedHotkey = hotkey;
        applyHotkey(GlobalHotkey::normalize(hotkey));
    }

    const QString iconChoice = cfg.value(QStringLiteral("app_icon")).toString(QStringLiteral("default"));
    if (iconChoice != m_iconChoice) applyIcon(iconChoice);
    cfg[QStringLiteral("app_icon")] = m_iconChoice;

    const bool startOnBoot = cfg.value(QStringLiteral("start_on_boot")).toBool(false);
    if (startOnBoot != m_appliedStartOnBoot) {
        m_appliedStartOnBoot = startOnBoot;
        AutoStart::setEnabled(startOnBoot);
    }

    m_config = cfg;
    ConfigStore::save(cfg);
}

void Application::quitApp() {
    saveConfig();
    if (m_window) m_window->stop();
    if (m_tray) m_tray->hide();
    quit();
}
