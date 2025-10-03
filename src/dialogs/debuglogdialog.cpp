#include "debuglogdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QKeyEvent>
#include <QClipboard>
#include <QApplication>
#include <QEvent>
#include <QMenu>

DebugLogDialog::DebugLogDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();

    // Connect to logger signals
    connect(&Logger::instance(), &Logger::logAdded,
            this, &DebugLogDialog::onLogAdded);
    connect(&Logger::instance(), &Logger::logUpdated,
            this, &DebugLogDialog::onLogUpdated);

    // Load existing logs
    refreshTable();
}

void DebugLogDialog::setupUI() {
    setWindowTitle("Debug Logs");
    resize(1200, 800);

    auto* mainLayout = new QVBoxLayout(this);

    // Top toolbar with filters
    auto* toolbarLayout = new QHBoxLayout();

    auto* filterLabel = new QLabel("Show:");
    toolbarLayout->addWidget(filterLabel);

    m_debugCheckBox = new QCheckBox("Debug");
    m_debugCheckBox->setChecked(false);
    connect(m_debugCheckBox, &QCheckBox::toggled, this, &DebugLogDialog::onFilterChanged);
    toolbarLayout->addWidget(m_debugCheckBox);

    m_infoCheckBox = new QCheckBox("Info");
    m_infoCheckBox->setChecked(true);
    connect(m_infoCheckBox, &QCheckBox::toggled, this, &DebugLogDialog::onFilterChanged);
    toolbarLayout->addWidget(m_infoCheckBox);

    m_warningCheckBox = new QCheckBox("Warning");
    m_warningCheckBox->setChecked(true);
    connect(m_warningCheckBox, &QCheckBox::toggled, this, &DebugLogDialog::onFilterChanged);
    toolbarLayout->addWidget(m_warningCheckBox);

    m_errorCheckBox = new QCheckBox("Error");
    m_errorCheckBox->setChecked(true);
    connect(m_errorCheckBox, &QCheckBox::toggled, this, &DebugLogDialog::onFilterChanged);
    toolbarLayout->addWidget(m_errorCheckBox);

    toolbarLayout->addSpacing(20);

    // Search box
    auto* searchLabel = new QLabel("Search:");
    toolbarLayout->addWidget(searchLabel);

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Filter by text...");
    m_searchEdit->setMinimumWidth(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DebugLogDialog::onSearchTextChanged);
    toolbarLayout->addWidget(m_searchEdit);

    toolbarLayout->addStretch();

    // Auto-scroll checkbox
    m_autoScrollCheckBox = new QCheckBox("Auto-scroll");
    m_autoScrollCheckBox->setChecked(true);
    connect(m_autoScrollCheckBox, &QCheckBox::toggled, this, &DebugLogDialog::onAutoScrollToggled);
    toolbarLayout->addWidget(m_autoScrollCheckBox);

    // Clear button
    m_clearButton = new QPushButton("Clear All");
    connect(m_clearButton, &QPushButton::clicked, this, &DebugLogDialog::onClearLogs);
    toolbarLayout->addWidget(m_clearButton);

    mainLayout->addLayout(toolbarLayout);

    // Table widget
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels({"Timestamp", "Level", "Source", "Message"});

    // Configure table appearance
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect context menu
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested,
            this, &DebugLogDialog::onShowContextMenu);

    // Set column widths
    m_tableWidget->setColumnWidth(0, 180); // Timestamp
    m_tableWidget->setColumnWidth(1, 80);  // Level
    m_tableWidget->setColumnWidth(2, 200); // Source

    mainLayout->addWidget(m_tableWidget);

    // Bottom buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

void DebugLogDialog::onLogAdded(const LogEntry& entry) {
    if (!shouldShowEntry(entry)) {
        return;
    }

    int row = m_tableWidget->rowCount();
    m_tableWidget->insertRow(row);
    addLogToTable(entry, row);

    // Store mapping for fast updates
    m_messageToRow[entry.message] = row;

    // Auto-scroll to bottom if enabled
    if (m_autoScroll) {
        m_tableWidget->scrollToBottom();
    }
}

void DebugLogDialog::onLogUpdated(int index, const LogEntry& entry) {
    Q_UNUSED(index);

    if (!shouldShowEntry(entry)) {
        return;
    }

    // Fast lookup using hash
    auto it = m_messageToRow.find(entry.message);
    if (it == m_messageToRow.end()) {
        return; // Not found (shouldn't happen)
    }

    int row = it.value();
    if (row < 0 || row >= m_tableWidget->rowCount()) {
        return; // Invalid row
    }

    // Update timestamp
    auto* timestampItem = m_tableWidget->item(row, 0);
    if (timestampItem) {
        timestampItem->setText(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    }

    // Update message with repeat count
    auto* messageItem = m_tableWidget->item(row, 3);
    if (messageItem) {
        QString messageText = entry.message;
        if (entry.repeatCount > 0) {
            messageText += QString(" (repeated %1x)").arg(entry.repeatCount + 1);
        }
        messageItem->setText(messageText);
    }
}

void DebugLogDialog::onFilterChanged() {
    refreshTable();
}

void DebugLogDialog::onClearLogs() {
    Logger::instance().clear();
    m_tableWidget->setRowCount(0);
    m_messageToRow.clear(); // Clear mapping
}

void DebugLogDialog::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    refreshTable();
}

void DebugLogDialog::onAutoScrollToggled(bool checked) {
    m_autoScroll = checked;
}

void DebugLogDialog::onShowContextMenu(const QPoint& pos) {
    QMenu contextMenu(this);

    QAction* copyAction = contextMenu.addAction("Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &DebugLogDialog::copySelectedToClipboard);

    // Only show menu if there are selected items
    if (!m_tableWidget->selectedItems().isEmpty()) {
        contextMenu.exec(m_tableWidget->mapToGlobal(pos));
    }
}

void DebugLogDialog::refreshTable() {
    // Save scroll position
    bool wasAtBottom = false;
    QScrollBar* scrollBar = m_tableWidget->verticalScrollBar();
    if (scrollBar) {
        wasAtBottom = scrollBar->value() == scrollBar->maximum();
    }

    m_tableWidget->setRowCount(0);
    m_messageToRow.clear(); // Clear mapping when refreshing

    auto entries = Logger::instance().getEntries();
    for (const auto& entry : entries) {
        if (!shouldShowEntry(entry)) {
            continue;
        }

        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);
        addLogToTable(entry, row);
        m_messageToRow[entry.message] = row; // Rebuild mapping
    }

    // Restore scroll position
    if (wasAtBottom && m_autoScroll) {
        m_tableWidget->scrollToBottom();
    }
}

void DebugLogDialog::addLogToTable(const LogEntry& entry, int row) {
    // Timestamp
    auto* timestampItem = new QTableWidgetItem(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    m_tableWidget->setItem(row, 0, timestampItem);

    // Level
    auto* levelItem = new QTableWidgetItem(levelToString(entry.level));
    levelItem->setForeground(QBrush(levelToColor(entry.level)));
    m_tableWidget->setItem(row, 1, levelItem);

    // Source
    auto* sourceItem = new QTableWidgetItem(entry.source);
    m_tableWidget->setItem(row, 2, sourceItem);

    // Message (with repeat count if > 0)
    QString messageText = entry.message;
    if (entry.repeatCount > 0) {
        messageText += QString(" (repeated %1x)").arg(entry.repeatCount + 1);
    }
    auto* messageItem = new QTableWidgetItem(messageText);
    m_tableWidget->setItem(row, 3, messageItem);
}

bool DebugLogDialog::shouldShowEntry(const LogEntry& entry) const {
    // Check level filters
    bool levelMatch = false;
    switch (entry.level) {
        case LogLevel::Debug:
            levelMatch = m_debugCheckBox->isChecked();
            break;
        case LogLevel::Info:
            levelMatch = m_infoCheckBox->isChecked();
            break;
        case LogLevel::Warning:
            levelMatch = m_warningCheckBox->isChecked();
            break;
        case LogLevel::Error:
            levelMatch = m_errorCheckBox->isChecked();
            break;
    }

    if (!levelMatch) {
        return false;
    }

    // Check search filter
    QString searchText = m_searchEdit->text();
    if (!searchText.isEmpty()) {
        bool textMatch = entry.message.contains(searchText, Qt::CaseInsensitive) ||
                        entry.source.contains(searchText, Qt::CaseInsensitive);
        if (!textMatch) {
            return false;
        }
    }

    return true;
}

QString DebugLogDialog::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

QColor DebugLogDialog::levelToColor(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug:   return QColor(128, 128, 128); // Gray
        case LogLevel::Info:    return QColor(0, 0, 0);       // Black
        case LogLevel::Warning: return QColor(255, 140, 0);   // Orange
        case LogLevel::Error:   return QColor(220, 20, 60);   // Red
        default:                return QColor(0, 0, 0);
    }
}

void DebugLogDialog::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Copy)) {
        copySelectedToClipboard();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool DebugLogDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_tableWidget && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            copySelectedToClipboard();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void DebugLogDialog::copySelectedToClipboard() {
    QList<QTableWidgetItem*> selectedItems = m_tableWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    // Get selected rows
    QSet<int> selectedRows;
    for (QTableWidgetItem* item : selectedItems) {
        selectedRows.insert(item->row());
    }

    // Convert to sorted list
    QList<int> rows = selectedRows.values();
    std::sort(rows.begin(), rows.end());

    // Build text to copy
    QString text;
    for (int row : rows) {
        QStringList rowData;
        for (int col = 0; col < m_tableWidget->columnCount(); ++col) {
            QTableWidgetItem* item = m_tableWidget->item(row, col);
            if (item) {
                rowData.append(item->text());
            }
        }
        text += rowData.join("\t") + "\n";
    }

    // Copy to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(text);
}