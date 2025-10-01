#include "dialogs/symbolsearchdialog.h"
#include "client/ibkrclient.h"
#include <QVBoxLayout>
#include <QKeyEvent>

SymbolSearchDialog::SymbolSearchDialog(IBKRClient *client, QWidget *parent)
    : QDialog(parent)
    , m_client(client)
{
    setWindowTitle("Symbol Search");
    resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search symbol (e.g. AAPL, TSLA)...");
    layout->addWidget(m_searchEdit);

    m_resultsWidget = new QListWidget(this);
    layout->addWidget(m_resultsWidget);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &SymbolSearchDialog::onSearchTextChanged);
    connect(m_resultsWidget, &QListWidget::itemActivated,
            this, &SymbolSearchDialog::onItemActivated);

    // Add some common symbols for now
    m_resultsWidget->addItem("AAPL - Apple Inc.");
    m_resultsWidget->addItem("TSLA - Tesla Inc.");
    m_resultsWidget->addItem("MSFT - Microsoft Corporation");
    m_resultsWidget->addItem("GOOGL - Alphabet Inc.");
    m_resultsWidget->addItem("AMZN - Amazon.com Inc.");
}

void SymbolSearchDialog::onSearchTextChanged(const QString& text)
{
    // TODO: Request symbol search from IBKR API
    m_resultsWidget->clear();

    if (text.isEmpty()) {
        m_resultsWidget->addItem("AAPL - Apple Inc.");
        m_resultsWidget->addItem("TSLA - Tesla Inc.");
        m_resultsWidget->addItem("MSFT - Microsoft Corporation");
        return;
    }

    // Simple filter for now
    QStringList symbols = {"AAPL", "TSLA", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "NFLX"};
    for (const QString& symbol : symbols) {
        if (symbol.contains(text, Qt::CaseInsensitive)) {
            m_resultsWidget->addItem(symbol);
        }
    }
}

void SymbolSearchDialog::onItemActivated(QListWidgetItem *item)
{
    QString symbol = item->text().split(" ").first();
    emit symbolSelected(symbol);
    accept();
}