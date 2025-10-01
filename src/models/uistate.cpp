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
            id INTEGER PRIMARY KEY,
            left_width INTEGER,
            center_width INTEGER,
            right_width INTEGER
        )
    )";

    if (!query.exec(createSplitterTable)) {
        qWarning() << "Failed to create splitter_state table:" << query.lastError().text();
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

void UIState::saveSplitterSizes(const QList<int>& sizes)
{
    if (sizes.size() != 3) {
        qWarning() << "Expected 3 splitter sizes, got" << sizes.size();
        return;
    }

    QSqlQuery query(m_db);

    // Delete existing record
    query.prepare("DELETE FROM splitter_state WHERE id = 1");
    query.exec();

    // Insert new record
    query.prepare(R"(
        INSERT INTO splitter_state (id, left_width, center_width, right_width)
        VALUES (1, :left_width, :center_width, :right_width)
    )");

    query.bindValue(":left_width", sizes[0]);
    query.bindValue(":center_width", sizes[1]);
    query.bindValue(":right_width", sizes[2]);

    if (!query.exec()) {
        qWarning() << "Failed to save splitter state:" << query.lastError().text();
    }
}

QList<int> UIState::restoreSplitterSizes()
{
    QSqlQuery query(m_db);
    query.prepare("SELECT left_width, center_width, right_width FROM splitter_state WHERE id = 1");

    if (query.exec() && query.next()) {
        QList<int> sizes;
        sizes << query.value(0).toInt();
        sizes << query.value(1).toInt();
        sizes << query.value(2).toInt();
        return sizes;
    }

    // Default sizes if not found (proportions: 1:4:2)
    return QList<int>() << 200 << 800 << 400;
}