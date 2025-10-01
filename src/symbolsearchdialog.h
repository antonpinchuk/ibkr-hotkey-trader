#ifndef SYMBOLSEARCHDIALOG_H
#define SYMBOLSEARCHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>

class IBKRClient;

class SymbolSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SymbolSearchDialog(IBKRClient *client, QWidget *parent = nullptr);

signals:
    void symbolSelected(const QString& symbol);

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem *item);

private:
    IBKRClient *m_client;
    QLineEdit *m_searchEdit;
    QListWidget *m_resultsWidget;
};

#endif // SYMBOLSEARCHDIALOG_H