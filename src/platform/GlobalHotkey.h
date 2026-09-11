#pragma once
#include <QObject>
#include <QString>

// Global hotkey via a low-level keyboard hook (WH_KEYBOARD_LL).
// Unlike RegisterHotKey, a hook is not subject to "hotkey already registered"
// conflicts with other applications, which is why the original Python version
// (using the `keyboard` library) worked.
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    bool registerShortcut(const QString& sequence);
    void unregisterShortcut();
    static QString normalize(const QString& sequence);

    // Called from the low-level keyboard hook (defined in the .cpp).
    bool matches(unsigned int vk, bool ctrl, bool alt, bool shift, bool meta) const;

signals:
    void activated();

private:
    void installHook();
    void removeHook();

    void* m_hook = nullptr;
    unsigned int m_vk = 0;
    bool m_ctrl = false;
    bool m_alt = false;
    bool m_shift = false;
    bool m_meta = false;
};
