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
class QTimeEdit;
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
    QComboBox* m_fontFamily = nullptr;
    QKeySequenceEdit* m_hotkeyEdit = nullptr;
    QComboBox* m_interval = nullptr;
    QComboBox* m_icon = nullptr;
    QComboBox* m_showMode = nullptr;
    QComboBox* m_edgeSide = nullptr;
    QTimeEdit* m_showStart = nullptr;
    QTimeEdit* m_showEnd = nullptr;
    QComboBox* m_fetchMode = nullptr;
    QTimeEdit* m_fetchStart = nullptr;
    QTimeEdit* m_fetchEnd = nullptr;
    int m_builtTabs = 0;
};
