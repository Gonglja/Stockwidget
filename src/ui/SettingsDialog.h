#pragma once
#include <QDialog>
#include <QJsonObject>

class FloatWindow;
class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QTabWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(FloatWindow* win, QWidget* parent = nullptr);
    static SettingsDialog* showFor(FloatWindow* win, QWidget* parent = nullptr);

private:
    QWidget* buildCodesTab();
    QWidget* buildDataTab();
    QWidget* buildAppearanceTab();
    QWidget* buildGeneralTab();
    void ensureTab(int index);
    void pickForeground();
    void pickBackground();
    void pickIcon();

    FloatWindow* m_win = nullptr;
    QTabWidget* m_tabs = nullptr;
    QListWidget* m_codeList = nullptr;
    QCheckBox* m_tableHeader = nullptr;
    QCheckBox* m_tableGrid = nullptr;
    QCheckBox* m_defaultColor = nullptr;
    QPushButton* m_fgButton = nullptr;
    QPushButton* m_bgButton = nullptr;
    QSlider* m_bgAlpha = nullptr;
    QSlider* m_winOpacity = nullptr;
    QSlider* m_fontSize = nullptr;
    QSlider* m_lineSpacing = nullptr;
    QSlider* m_padding = nullptr;
    QComboBox* m_fontFamily = nullptr;
    QKeySequenceEdit* m_hotkeyEdit = nullptr;
    QComboBox* m_interval = nullptr;
    QComboBox* m_icon = nullptr;
    int m_builtTabs = 0;
};
