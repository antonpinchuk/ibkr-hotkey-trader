#ifndef DEBUGLOGDIALOG_H
#define DEBUGLOGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include "../utils/logger.h"

class DebugLogDialog : public QDialog {
    Q_OBJECT

public:
    explicit DebugLogDialog(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onLogAdded(const LogEntry& entry);
    void onLogUpdated(int index, const LogEntry& entry);
    void onFilterChanged();
    void onClearLogs();
    void onSearchTextChanged(const QString& text);
    void onAutoScrollToggled(bool checked);
    void onShowContextMenu(const QPoint& pos);

private:
    void setupUI();
    void refreshTable();
    void addLogToTable(const LogEntry& entry, int row);
    bool shouldShowEntry(const LogEntry& entry) const;
    QString levelToString(LogLevel level) const;
    QColor levelToColor(LogLevel level) const;
    void copySelectedToClipboard();

    QTableWidget* m_tableWidget;
    QCheckBox* m_debugCheckBox;
    QCheckBox* m_infoCheckBox;
    QCheckBox* m_warningCheckBox;
    QCheckBox* m_errorCheckBox;
    QCheckBox* m_autoScrollCheckBox;
    QPushButton* m_clearButton;
    QLineEdit* m_searchEdit;

    bool m_autoScroll = true;
    QHash<QString, int> m_messageToRow; // Fast lookup: message -> table row
};

#endif // DEBUGLOGDIALOG_H