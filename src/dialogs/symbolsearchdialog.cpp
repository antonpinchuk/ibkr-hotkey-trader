#include "dialogs/symbolsearchdialog.h"
#include "client/ibkrclient.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QShowEvent>
#include <QLabel>
#include <QFontMetrics>

// SearchResultDelegate implementation
void SearchResultDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Draw background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
        painter->setPen(option.palette.highlightedText().color());
    } else {
        if (index.row() % 2 == 0) {
            painter->fillRect(option.rect, option.palette.base());
        } else {
            painter->fillRect(option.rect, option.palette.alternateBase());
        }
        painter->setPen(option.palette.text().color());
    }

    if (index.row() >= m_results->size()) {
        // Fallback for "Searching..." or "No results" messages
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const SymbolSearchResult& result = m_results->at(index.row());

    QFont boldFont = option.font;
    boldFont.setBold(true);

    int margin = 8;
    int symbolWidth = 80;
    int exchangeWidth = 100;

    // Draw symbol (bold, left)
    painter->setFont(boldFont);
    QRect symbolRect = option.rect.adjusted(margin, 0, 0, 0);
    symbolRect.setWidth(symbolWidth);
    painter->drawText(symbolRect, Qt::AlignLeft | Qt::AlignVCenter, result.symbol);

    // Draw company name (center)
    painter->setFont(option.font);
    QRect companyRect = option.rect.adjusted(margin + symbolWidth, 0, -exchangeWidth - margin, 0);
    QString elidedCompany = option.fontMetrics.elidedText(result.companyName, Qt::ElideRight, companyRect.width());
    painter->drawText(companyRect, Qt::AlignLeft | Qt::AlignVCenter, elidedCompany);

    // Draw exchange (right aligned)
    QFont exchangeFont = option.font;
    exchangeFont.setItalic(true);
    painter->setFont(exchangeFont);
    QRect exchangeRect = option.rect.adjusted(0, 0, -margin, 0);
    painter->drawText(exchangeRect, Qt::AlignRight | Qt::AlignVCenter, result.exchange);
}

QSize SearchResultDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QSize(option.rect.width(), 30);
}

// SymbolSearchDialog implementation
SymbolSearchDialog::SymbolSearchDialog(IBKRClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_currentReqId(1000)
    , m_pendingEnterReqId(-1)
    , m_pendingEnter(false)
{
    setWindowTitle("Symbol Search");
    resize(700, 450);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search symbol (e.g. AAPL, TSLA)...");
    m_searchEdit->installEventFilter(this);
    layout->addWidget(m_searchEdit);

    m_resultsWidget = new QListWidget(this);
    m_resultsWidget->setAlternatingRowColors(true);

    // Set custom delegate
    m_delegate = new SearchResultDelegate(&m_searchResults, this);
    m_resultsWidget->setItemDelegate(m_delegate);

    layout->addWidget(m_resultsWidget);

    // Setup search timer for debounce
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(450); // 450ms debounce

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &SymbolSearchDialog::onSearchTextChanged);
    connect(m_resultsWidget, &QListWidget::itemActivated,
            this, &SymbolSearchDialog::onItemActivated);
    connect(m_searchTimer, &QTimer::timeout,
            this, &SymbolSearchDialog::onSearchTimeout);
    connect(m_client, &IBKRClient::symbolSearchResultsReceived,
            this, &SymbolSearchDialog::onSymbolSearchResults);

    m_searchEdit->setFocus();
}

void SymbolSearchDialog::onSearchTextChanged(const QString& text)
{
    m_searchTimer->stop();
    m_pendingEnter = false;  // Clear pending enter on new input

    if (text.isEmpty()) {
        m_resultsWidget->clear();
        m_searchResults.clear();
        return;
    }

    // Restart timer for debounced search
    m_searchTimer->start();
}

void SymbolSearchDialog::onSearchTimeout()
{
    QString searchText = m_searchEdit->text().trimmed();
    if (searchText.isEmpty()) return;

    performSearch();
}

void SymbolSearchDialog::performSearch()
{
    QString searchText = m_searchEdit->text().trimmed();
    if (searchText.isEmpty()) return;

    // Don't clear list - keep previous results while searching
    m_client->searchSymbol(m_currentReqId++, searchText);
}

void SymbolSearchDialog::onSymbolSearchResults(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results)
{
    // Check if this is an old result we don't care about anymore
    if (m_pendingEnter && reqId != m_pendingEnterReqId) {
        // Ignore old results, waiting for the Enter request
        return;
    }

    m_resultsWidget->clear();
    m_searchResults.clear();

    if (results.isEmpty()) {
        m_resultsWidget->addItem("No results found");
        if (reqId == m_pendingEnterReqId) {
            m_pendingEnter = false;  // Clear pending enter
            m_pendingEnterReqId = -1;
        }
        return;
    }

    for (const auto& result : results) {
        SymbolSearchResult searchResult;
        searchResult.symbol = result.first;
        searchResult.companyName = result.second.first;
        searchResult.exchange = result.second.second;
        m_searchResults.append(searchResult);

        // Add item (delegate will handle formatting)
        m_resultsWidget->addItem("");
    }

    // Select first item
    if (m_resultsWidget->count() > 0) {
        m_resultsWidget->setCurrentRow(0);

        // If this is the request we're waiting for after Enter, auto-select first result
        if (m_pendingEnter && reqId == m_pendingEnterReqId) {
            m_pendingEnter = false;
            m_pendingEnterReqId = -1;
            QListWidgetItem *firstItem = m_resultsWidget->item(0);
            if (firstItem) {
                onItemActivated(firstItem);
            }
        }
    }
}

void SymbolSearchDialog::onItemActivated(QListWidgetItem *item)
{
    int index = m_resultsWidget->row(item);
    if (index >= 0 && index < m_searchResults.size()) {
        emit symbolSelected(m_searchResults[index].symbol, m_searchResults[index].exchange);
        accept();
    }
}

bool SymbolSearchDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Down) {
            // Move to list if not empty
            if (m_resultsWidget->count() > 0) {
                int currentRow = m_resultsWidget->currentRow();
                if (currentRow < 0) {
                    m_resultsWidget->setCurrentRow(0);
                } else if (currentRow < m_resultsWidget->count() - 1) {
                    m_resultsWidget->setCurrentRow(currentRow + 1);
                }
            }
            return true; // Keep focus on search edit
        } else if (keyEvent->key() == Qt::Key_Up) {
            // Move up in list
            if (m_resultsWidget->count() > 0) {
                int currentRow = m_resultsWidget->currentRow();
                if (currentRow > 0) {
                    m_resultsWidget->setCurrentRow(currentRow - 1);
                }
            }
            return true; // Keep focus on search edit
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            QString searchText = m_searchEdit->text().trimmed();

            // Select current item if exists and matches current search text
            QListWidgetItem *currentItem = m_resultsWidget->currentItem();
            if (currentItem && m_resultsWidget->count() > 0 && !m_searchResults.isEmpty()) {
                // Check if first result matches search text (approximate match)
                if (!m_searchResults.isEmpty() &&
                    m_searchResults[0].symbol.startsWith(searchText, Qt::CaseInsensitive)) {
                    onItemActivated(currentItem);
                    return true;
                }
            }

            // No matching results or text changed - trigger immediate search
            if (!searchText.isEmpty()) {
                m_searchTimer->stop();  // Cancel pending search
                m_pendingEnter = true;
                m_pendingEnterReqId = m_currentReqId;  // Remember which request we're waiting for
                performSearch();  // Start search immediately
                return true;
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void SymbolSearchDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // Select all text when dialog is shown
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}