#include "logger.h"
#include <QMutexLocker>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::log(LogLevel level, const QString& message, const QString& source) {
    QMutexLocker locker(&m_mutex);

    // Check for duplicate within time window
    QDateTime now = QDateTime::currentDateTime();
    int checkCount = qMin(20, m_entries.size());

    for (int i = m_entries.size() - 1; i >= m_entries.size() - checkCount && i >= 0; --i) {
        LogEntry& entry = m_entries[i];

        // Check if entry is within time window
        qint64 msSinceEntry = entry.timestamp.msecsTo(now);
        if (msSinceEntry > DUPLICATE_WINDOW_MS) {
            break; // Older entries won't be duplicates
        }

        // Check if message matches
        if (entry.message == message) {
            // Found duplicate - increment repeat count and update timestamp
            entry.repeatCount++;
            entry.timestamp = now;
            emit logUpdated(i, entry);
            return; // Don't add new entry
        }
    }

    // Not a duplicate - add new entry
    LogEntry entry;
    entry.timestamp = now;
    entry.level = level;
    entry.message = message;
    entry.source = source;
    entry.repeatCount = 0;

    // Keep only last MAX_ENTRIES to prevent memory overflow
    if (m_entries.size() >= MAX_ENTRIES) {
        m_entries.removeFirst();
    }

    m_entries.append(entry);

    emit logAdded(entry);
}

void Logger::debug(const QString& message, const QString& source) {
    log(LogLevel::Debug, message, source);
}

void Logger::info(const QString& message, const QString& source) {
    log(LogLevel::Info, message, source);
}

void Logger::warning(const QString& message, const QString& source) {
    log(LogLevel::Warning, message, source);
}

void Logger::error(const QString& message, const QString& source) {
    log(LogLevel::Error, message, source);
}

QVector<LogEntry> Logger::getEntries() const {
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

void Logger::clear() {
    QMutexLocker locker(&m_mutex);
    m_entries.clear();
}