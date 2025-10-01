#ifndef UISTATE_H
#define UISTATE_H

#include <QObject>
#include <QRect>
#include <QList>
#include <QSqlDatabase>

class UIState : public QObject
{
    Q_OBJECT

public:
    static UIState& instance();

    // Window geometry
    void saveWindowGeometry(const QRect& geometry, bool isMaximized, const QString& screenName);
    QRect restoreWindowGeometry(bool& isMaximized, QString& screenName);

    // Splitter state
    void saveSplitterSizes(const QList<int>& sizes);
    QList<int> restoreSplitterSizes();

private:
    explicit UIState(QObject *parent = nullptr);
    ~UIState();

    void initDatabase();
    void createTables();

    QSqlDatabase m_db;
};

#endif // UISTATE_H