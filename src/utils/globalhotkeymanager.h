#ifndef GLOBALHOTKEYMANAGER_H
#define GLOBALHOTKEYMANAGER_H

#include <QObject>
#include <QMap>

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#endif

class GlobalHotkeyManager : public QObject
{
    Q_OBJECT

public:
    enum HotkeyAction {
        Open100,
        Open50,
        Add5,
        Add10,
        Add15,
        Add20,
        Add25,
        Add30,
        Add35,
        Add40,
        Add45,
        Add50,
        Close25,
        Close50,
        Close75,
        Close100,
        CancelOrders
    };

    explicit GlobalHotkeyManager(QObject *parent = nullptr);
    ~GlobalHotkeyManager();

    bool registerHotkeys();
    void unregisterHotkeys();

signals:
    void hotkeyPressed(HotkeyAction action);

private:
#ifdef Q_OS_MAC
    struct HotkeyInfo {
        EventHotKeyRef ref;
        HotkeyAction action;
        uint32_t hotkeyId;
    };

    static OSStatus hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void* userData);

    bool registerHotkey(uint32_t modifiers, uint32_t keyCode, HotkeyAction action);

    QMap<uint32_t, HotkeyInfo> m_hotkeys;
    EventHandlerRef m_eventHandler;
    uint32_t m_nextHotkeyId;

    static const uint32_t kHotkeySignature = 'IBKR';
#endif
};

#endif // GLOBALHOTKEYMANAGER_H
