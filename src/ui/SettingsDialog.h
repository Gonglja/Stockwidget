#pragma once
#include <QDialog>
#include <QJsonObject>

class FloatWindow;
class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSlider;
class QTimeEdit;
class QTimer;
class QTabWidget;
class StockSuggestSource;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(FloatWindow* win, QWidget* parent = nullptr);
    static SettingsDialog* showFor(FloatWindow* win, QWidget* parent = nullptr);

    // 校验并落库一条「代码 + 别名」；item 为空则按代码查找/新建。非法代码返回 false
    bool applyCodeEdit(const QString& code, const QString& alias, QListWidgetItem* item = nullptr);

private:
    QWidget* buildCodesTab();
    QWidget* buildDataTab();
    QWidget* buildAppearanceTab();
    QWidget* buildGeneralTab();
    void ensureTab(int index);
    void commitCodes();
    void editCodeItem(QListWidgetItem* item);
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
    QComboBox* m_nameLength = nullptr;
    QComboBox* m_icon = nullptr;
    QComboBox* m_showMode = nullptr;
    QComboBox* m_edgeSide = nullptr;
    QTimeEdit* m_showStart = nullptr;
    QTimeEdit* m_showEnd = nullptr;
    QComboBox* m_fetchMode = nullptr;
    QTimeEdit* m_fetchStart = nullptr;
    QTimeEdit* m_fetchEnd = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_suggestList = nullptr;
    QTimer* m_suggestDebounce = nullptr;
    StockSuggestSource* m_suggest = nullptr;
    int m_builtTabs = 0;
};
