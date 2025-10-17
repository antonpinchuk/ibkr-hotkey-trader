#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMutex>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level;
    QString message;
    QString source; // Optional: class/function name
    int repeatCount = 0; // Number of times this message was repeated
};

class Logger : public QObject {
    Q_OBJECT

public:
    static Logger& instance();

    void log(LogLevel level, const QString& message, const QString& source = QString());
    void debug(const QString& message, const QString& source = QString());
    void info(const QString& message, const QString& source = QString());
    void warning(const QString& message, const QString& source = QString());
    void error(const QString& message, const QString& source = QString());

    QVector<LogEntry> getEntries() const;
    void clear();

signals:
    void logAdded(const LogEntry& entry);
    void logUpdated(int index, const LogEntry& entry);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QVector<LogEntry> m_entries;
    mutable QMutex m_mutex;
    static constexpr int MAX_ENTRIES = 50000; // Large buffer for day trading (tick-by-tick updates)
    static constexpr int DUPLICATE_WINDOW_MS = 2000; // Check duplicates within 2 seconds
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FUNCTION__)
#define LOG_INFO(msg) Logger::instance().info(msg, __FUNCTION__)
#define LOG_WARNING(msg) Logger::instance().warning(msg, __FUNCTION__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FUNCTION__)

#endif // LOGGER_H