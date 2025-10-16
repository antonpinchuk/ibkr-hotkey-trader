#include "models/uistate.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QVariant>

UIState& UIState::instance()
{
    static UIState instance;
    return instance;
}

UIState::UIState(QObject *parent)
    : QObject(parent)
{
    initDatabase();
}

UIState::~UIState()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void UIState::initDatabase()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString dbPath = dataPath + "/uistate.db";
    qDebug() << "UI State database path:" << dbPath;

    m_db = QSqlDatabase::addDatabase("QSQLITE", "uistate_connection");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open UI state database:" << m_db.lastError().text();
        return;
    }

    createTables();
}

void UIState::createTables()
{
    QSqlQuery query(m_db);

    // Window geometry table
    QString createWindowTable = R"(
        CREATE TABLE IF NOT EXISTS window_geometry (
            id INTEGER PRIMARY KEY,
            x INTEGER,
            y INTEGER,
            width INTEGER,
            height INTEGER,
            is_maximized INTEGER,
            screen_name TEXT
        )
    )";

    if (!query.exec(createWindowTable)) {
        qWarning() << "Failed to create window_geometry table:" << query.lastError().text();
    }

    // Splitter state table
    QString createSplitterTable = R"(
        CREATE TABLE IF NOT EXISTS splitter_state (
            splitter_name TEXT PRIMARY KEY,
            sizes TEXT
        )
    )";

    if (!query.exec(createSplitterTable)) {
        qWarning() << "Failed to create splitter_state table:" << query.lastError().text();
    }

    // Table column widths table
    QString createTableWidthsTable = R"(
        CREATE TABLE IF NOT EXISTS table_column_widths (
            table_name TEXT PRIMARY KEY,
            widths TEXT
        )
    )";

    if (!query.exec(createTableWidthsTable)) {
        qWarning() << "Failed to create table_column_widths table:" << query.lastError().text();
    }

    // Chart zoom state table (per timeframe)
    QString createChartZoomTable = R"(
        CREATE TABLE IF NOT EXISTS chart_zoom (
            timeframe TEXT PRIMARY KEY,
            lower REAL,
            upper REAL
        )
    )";

    if (!query.exec(createChartZoomTable)) {
        qWarning() << "Failed to create chart_zoom table:" << query.lastError().text();
    }
}

void UIState::saveWindowGeometry(const QRect& geometry, bool isMaximized, const QString& screenName)
{
    QSqlQuery query(m_db);

    // Delete existing record
    query.prepare("DELETE FROM window_geometry WHERE id = 1");
    query.exec();

    // Insert new record
    query.prepare(R"(
        INSERT INTO window_geometry (id, x, y, width, height, is_maximized, screen_name)
        VALUES (1, :x, :y, :width, :height, :is_maximized, :screen_name)
    )");

    query.bindValue(":x", geometry.x());
    query.bindValue(":y", geometry.y());
    query.bindValue(":width", geometry.width());
    query.bindValue(":height", geometry.height());
    query.bindValue(":is_maximized", isMaximized ? 1 : 0);
    query.bindValue(":screen_name", screenName);

    if (!query.exec()) {
        qWarning() << "Failed to save window geometry:" << query.lastError().text();
    }
}

QRect UIState::restoreWindowGeometry(bool& isMaximized, QString& screenName)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT x, y, width, height, is_maximized, screen_name FROM window_geometry WHERE id = 1");

    if (query.exec() && query.next()) {
        int x = query.value(0).toInt();
        int y = query.value(1).toInt();
        int width = query.value(2).toInt();
        int height = query.value(3).toInt();
        isMaximized = query.value(4).toInt() == 1;
        screenName = query.value(5).toString();

        return QRect(x, y, width, height);
    }

    // Default geometry if not found
    isMaximized = false;
    screenName = "";
    return QRect(100, 100, 1400, 800);
}

void UIState::saveSplitterSizes(const QString& splitterName, const QList<int>& sizes)
{
    QSqlQuery query(m_db);

    // Convert sizes to comma-separated string
    QStringList sizeStrings;
    for (int size : sizes) {
        sizeStrings << QString::number(size);
    }
    QString sizesStr = sizeStrings.join(",");

    // Delete existing record
    query.prepare("DELETE FROM splitter_state WHERE splitter_name = :splitter_name");
    query.bindValue(":splitter_name", splitterName);
    query.exec();

    // Insert new record
    query.prepare(R"(
        INSERT INTO splitter_state (splitter_name, sizes)
        VALUES (:splitter_name, :sizes)
    )");

    query.bindValue(":splitter_name", splitterName);
    query.bindValue(":sizes", sizesStr);

    if (!query.exec()) {
        qWarning() << "Failed to save splitter state:" << query.lastError().text();
    }
}

QList<int> UIState::restoreSplitterSizes(const QString& splitterName)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT sizes FROM splitter_state WHERE splitter_name = :splitter_name");
    query.bindValue(":splitter_name", splitterName);

    if (query.exec() && query.next()) {
        QString sizesStr = query.value(0).toString();
        QStringList sizeStrings = sizesStr.split(",");

        QList<int> sizes;
        for (const QString& sizeStr : sizeStrings) {
            sizes << sizeStr.toInt();
        }
        return sizes;
    }

    // Return empty list if not found - caller will use defaults
    return QList<int>();
}

void UIState::saveTableColumnWidths(const QString& tableName, const QList<int>& widths)
{
    QSqlQuery query(m_db);

    // Convert widths to comma-separated string
    QStringList widthStrings;
    for (int width : widths) {
        widthStrings << QString::number(width);
    }
    QString widthsStr = widthStrings.join(",");

    // Delete existing record
    query.prepare("DELETE FROM table_column_widths WHERE table_name = :table_name");
    query.bindValue(":table_name", tableName);
    query.exec();

    // Insert new record
    query.prepare(R"(
        INSERT INTO table_column_widths (table_name, widths)
        VALUES (:table_name, :widths)
    )");

    query.bindValue(":table_name", tableName);
    query.bindValue(":widths", widthsStr);

    if (!query.exec()) {
        qWarning() << "Failed to save table column widths:" << query.lastError().text();
    }
}

QList<int> UIState::restoreTableColumnWidths(const QString& tableName)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT widths FROM table_column_widths WHERE table_name = :table_name");
    query.bindValue(":table_name", tableName);

    if (query.exec() && query.next()) {
        QString widthsStr = query.value(0).toString();
        QStringList widthStrings = widthsStr.split(",");

        QList<int> widths;
        for (const QString& widthStr : widthStrings) {
            widths << widthStr.toInt();
        }
        return widths;
    }

    // Return empty list if not found
    return QList<int>();
}

void UIState::saveChartZoom(const QString& timeframe, double lower, double upper)
{
    QSqlQuery query(m_db);

    // Delete existing record
    query.prepare("DELETE FROM chart_zoom WHERE timeframe = :timeframe");
    query.bindValue(":timeframe", timeframe);
    query.exec();

    // Insert new record
    query.prepare(R"(
        INSERT INTO chart_zoom (timeframe, lower, upper)
        VALUES (:timeframe, :lower, :upper)
    )");

    query.bindValue(":timeframe", timeframe);
    query.bindValue(":lower", lower);
    query.bindValue(":upper", upper);

    if (!query.exec()) {
        qWarning() << "Failed to save chart zoom:" << query.lastError().text();
    }
}

bool UIState::restoreChartZoom(const QString& timeframe, double& lower, double& upper)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT lower, upper FROM chart_zoom WHERE timeframe = :timeframe");
    query.bindValue(":timeframe", timeframe);

    if (query.exec() && query.next()) {
        lower = query.value(0).toDouble();
        upper = query.value(1).toDouble();
        return true;
    }

    return false;
}