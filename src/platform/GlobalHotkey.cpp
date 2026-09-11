#include "platform/GlobalHotkey.h"
#include <QKeySequence>
#include <windows.h>

namespace {

GlobalHotkey* g_active = nullptr;

unsigned int vkFromKey(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return static_cast<unsigned int>('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return static_cast<unsigned int>('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return static_cast<unsigned int>(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
        case Qt::Key_Space: return VK_SPACE;
        case Qt::Key_Tab: return VK_TAB;
        case Qt::Key_Return:
        case Qt::Key_Enter: return VK_RETURN;
        case Qt::Key_Escape: return VK_ESCAPE;
        case Qt::Key_Backspace: return VK_BACK;
        case Qt::Key_Delete: return VK_DELETE;
        case Qt::Key_Insert: return VK_INSERT;
        case Qt::Key_Home: return VK_HOME;
        case Qt::Key_End: return VK_END;
        case Qt::Key_PageUp: return VK_PRIOR;
        case Qt::Key_PageDown: return VK_NEXT;
        case Qt::Key_Left: return VK_LEFT;
        case Qt::Key_Right: return VK_RIGHT;
        case Qt::Key_Up: return VK_UP;
        case Qt::Key_Down: return VK_DOWN;
        case Qt::Key_Print: return VK_SNAPSHOT;
        default: return 0;
    }
}

LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_active && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool meta = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;
        if (g_active->matches(kb->vkCode, ctrl, alt, shift, meta)) {
            GlobalHotkey* self = g_active;
            QMetaObject::invokeMethod(self, [self] { emit self->activated(); },
                                      Qt::QueuedConnection);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {}

GlobalHotkey::~GlobalHotkey() { removeHook(); }

QString GlobalHotkey::normalize(const QString& sequence) {
    return QKeySequence(sequence, QKeySequence::PortableText)
        .toString(QKeySequence::PortableText);
}

bool GlobalHotkey::registerShortcut(const QString& sequence) {
    unregisterShortcut();
    const QKeySequence seq(sequence, QKeySequence::PortableText);
    if (seq.isEmpty()) return false;

    const int combined = seq[0].toCombined();
    m_vk = vkFromKey(combined & 0x01FFFFFF);
    m_ctrl = (combined & Qt::CTRL) != 0;
    m_alt = (combined & Qt::ALT) != 0;
    m_shift = (combined & Qt::SHIFT) != 0;
    m_meta = (combined & Qt::META) != 0;
    if (m_vk == 0) return false;

    installHook();
    if (!m_hook) {
        m_vk = 0;
        return false;
    }
    return true;
}

void GlobalHotkey::unregisterShortcut() {
    removeHook();
    m_vk = 0;
}

bool GlobalHotkey::matches(unsigned int vk, bool ctrl, bool alt, bool shift, bool meta) const {
    return m_vk != 0 && vk == m_vk && ctrl == m_ctrl && alt == m_alt && shift == m_shift &&
           meta == m_meta;
}

void GlobalHotkey::installHook() {
    if (m_hook) return;
    g_active = this;
    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, &lowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    if (!m_hook && g_active == this) g_active = nullptr;
}

void GlobalHotkey::removeHook() {
    if (m_hook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_hook));
        m_hook = nullptr;
    }
    if (g_active == this) g_active = nullptr;
}
