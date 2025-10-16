#ifndef SYSTEMTRAYMANAGER_H
#define SYSTEMTRAYMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>

#ifdef Q_OS_MAC
// Forward declarations for Objective-C objects
#ifdef __OBJC__
@class NSStatusItem;
#else
typedef struct objc_object NSStatusItem;
#endif
#endif

class SystemTrayManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemTrayManager(QObject *parent = nullptr);
    ~SystemTrayManager();

    void setTickerSymbol(const QString& symbol);
    void startBlinking();
    void stopBlinking();
    bool isBlinking() const { return m_isBlinking; }

private slots:
    void onBlinkTimer();

private:
    void updateTrayDisplay();
    void createStatusItem();
    void destroyStatusItem();

#ifdef Q_OS_MAC
    NSStatusItem* m_statusItem;
#endif

    QString m_tickerSymbol;
    bool m_isBlinking;
    bool m_blinkVisible;
    QTimer* m_blinkTimer;
};

#endif // SYSTEMTRAYMANAGER_H
