#pragma once
#include <QDialog>

class FloatWindow;
class QComboBox;
class QShowEvent;
class QTableWidget;

// 浮窗右下角「配置」按钮打开的对话框：排序 + 自定义名称。
// 非模态单例（showFor），每次显示都从当前配置重建，避免与设置面板/右键菜单不同步。
class CustomConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomConfigDialog(FloatWindow* win, QWidget* parent = nullptr);
    static CustomConfigDialog* showFor(FloatWindow* win, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void rebuildSortRow();
    void rebuildAliasTable();
    void commitSort();
    void commitAliases();

    FloatWindow* m_win = nullptr;
    QComboBox* m_sortKey = nullptr;
    QComboBox* m_sortDir = nullptr;
    QTableWidget* m_aliasTable = nullptr;
    bool m_loading = false;
};
