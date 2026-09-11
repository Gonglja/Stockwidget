#pragma once
#include <QApplication>
#include <QIcon>
#include <QJsonObject>

class FloatWindow;
class GlobalHotkey;
class QSystemTrayIcon;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    void applyHotkey(const QString& sequence);
    void applyIcon(const QString& choice);
    void quitApp();

private:
    void saveConfig();
    QIcon resolveIcon(const QString& choice) const;
    void openSettings();
    void toggleWindow();
    void locateWindow();

    QJsonObject m_config;
    FloatWindow* m_window = nullptr;
    QSystemTrayIcon* m_tray = nullptr;
    GlobalHotkey* m_hotkey = nullptr;
    QString m_iconChoice;
    QString m_appliedHotkey;
    bool m_appliedStartOnBoot = false;
};
