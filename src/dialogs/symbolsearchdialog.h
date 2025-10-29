#ifndef SYMBOLSEARCHDIALOG_H
#define SYMBOLSEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QPainter>

class IBKRClient;
class SymbolSearchManager;

struct SymbolSearchResult {
    QString symbol;
    QString companyName;
    QString exchange;
    int conId;

    SymbolSearchResult() : conId(0) {}
};

// Custom delegate for search results
class SearchResultDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SearchResultDelegate(const QList<SymbolSearchResult>* results, QObject *parent = nullptr)
        : QStyledItemDelegate(parent), m_results(results) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    const QList<SymbolSearchResult>* m_results;
};

class SymbolSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SymbolSearchDialog(SymbolSearchManager *searchManager, QWidget *parent = nullptr);

signals:
    void symbolSelected(const QString& symbol, const QString& exchange, int conId = 0);

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem *item);
    void onSearchTimeout();
    void onSymbolSearchResults(int reqId, const QList<QPair<QString, QPair<QString, QString>>>& results, const QMap<QString, int>& symbolToConId);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void performSearch();

    SymbolSearchManager *m_searchManager;
    QLineEdit *m_searchEdit;
    QListWidget *m_resultsWidget;
    QTimer *m_searchTimer;
    int m_currentReqId;
    int m_pendingEnterReqId;  // Request ID we're waiting for after Enter
    QList<SymbolSearchResult> m_searchResults;
    SearchResultDelegate *m_delegate;
    bool m_pendingEnter;  // True if user pressed Enter while searching
};

#endif // SYMBOLSEARCHDIALOG_H