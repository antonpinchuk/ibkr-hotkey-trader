#include "utils/globalhotkeymanager.h"
#include "utils/logger.h"

#ifdef Q_OS_MAC

GlobalHotkeyManager::GlobalHotkeyManager(QObject *parent)
    : QObject(parent)
    , m_eventHandler(nullptr)
    , m_nextHotkeyId(1)
{
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    unregisterHotkeys();
}

bool GlobalHotkeyManager::registerHotkeys()
{
    LOG_DEBUG("Registering global hotkeys");

    // Install event handler
    EventTypeSpec eventType;
    eventType.eventClass = kEventClassKeyboard;
    eventType.eventKind = kEventHotKeyPressed;

    OSStatus status = InstallEventHandler(
        GetApplicationEventTarget(),
        &GlobalHotkeyManager::hotkeyHandler,
        1,
        &eventType,
        this,
        &m_eventHandler
    );

    if (status != noErr) {
        LOG_ERROR(QString("Failed to install event handler: %1").arg(status));
        return false;
    }

    // Register hotkeys with their key codes
    // macOS virtual key codes: https://eastmanreference.com/complete-list-of-applescript-key-codes

    // Shift+Control+Option+O (Open 100%)
    registerHotkey(shiftKey | controlKey | optionKey, 31, Open100); // O = 31

    // Shift+Control+Option+P (Open 50%)
    registerHotkey(shiftKey | controlKey | optionKey, 35, Open50); // P = 35

    // Shift+Control+Option+1-0 (Add 5%-50%)
    registerHotkey(shiftKey | controlKey | optionKey, 18, Add5);  // 1 = 18
    registerHotkey(shiftKey | controlKey | optionKey, 19, Add10); // 2 = 19
    registerHotkey(shiftKey | controlKey | optionKey, 20, Add15); // 3 = 20
    registerHotkey(shiftKey | controlKey | optionKey, 21, Add20); // 4 = 21
    registerHotkey(shiftKey | controlKey | optionKey, 23, Add25); // 5 = 23
    registerHotkey(shiftKey | controlKey | optionKey, 22, Add30); // 6 = 22
    registerHotkey(shiftKey | controlKey | optionKey, 26, Add35); // 7 = 26
    registerHotkey(shiftKey | controlKey | optionKey, 28, Add40); // 8 = 28
    registerHotkey(shiftKey | controlKey | optionKey, 25, Add45); // 9 = 25
    registerHotkey(shiftKey | controlKey | optionKey, 29, Add50); // 0 = 29

    // Control+Option+Z (Close 100%)
    registerHotkey(controlKey | optionKey, 6, Close100); // Z = 6

    // Control+Option+X (Close 75%)
    registerHotkey(controlKey | optionKey, 7, Close75); // X = 7

    // Control+Option+C (Close 50%)
    registerHotkey(controlKey | optionKey, 8, Close50); // C = 8

    // Control+Option+V (Close 25%)
    registerHotkey(controlKey | optionKey, 9, Close25); // V = 9

    // Control+Option+Q (Cancel Orders)
    registerHotkey(controlKey | optionKey, 12, CancelOrders); // Q = 12

    LOG_DEBUG(QString("Registered %1 global hotkeys").arg(m_hotkeys.size()));
    return true;
}

void GlobalHotkeyManager::unregisterHotkeys()
{
    LOG_DEBUG("Unregistering global hotkeys");

    // Unregister all hotkeys
    for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
        UnregisterEventHotKey(it.value().ref);
    }
    m_hotkeys.clear();

    // Remove event handler
    if (m_eventHandler) {
        RemoveEventHandler(m_eventHandler);
        m_eventHandler = nullptr;
    }

    LOG_DEBUG("Global hotkeys unregistered");
}

bool GlobalHotkeyManager::registerHotkey(uint32_t modifiers, uint32_t keyCode, HotkeyAction action)
{
    EventHotKeyID hotkeyId;
    hotkeyId.signature = kHotkeySignature;
    hotkeyId.id = m_nextHotkeyId++;

    EventHotKeyRef hotkeyRef;
    OSStatus status = RegisterEventHotKey(
        keyCode,
        modifiers,
        hotkeyId,
        GetApplicationEventTarget(),
        0,
        &hotkeyRef
    );

    if (status != noErr) {
        LOG_ERROR(QString("Failed to register hotkey (key=%1, modifiers=%2): %3")
            .arg(keyCode).arg(modifiers).arg(status));
        return false;
    }

    HotkeyInfo info;
    info.ref = hotkeyRef;
    info.action = action;
    info.hotkeyId = hotkeyId.id;

    m_hotkeys[hotkeyId.id] = info;

    LOG_DEBUG(QString("Registered hotkey: id=%1, key=%2, modifiers=%3, action=%4")
        .arg(hotkeyId.id).arg(keyCode).arg(modifiers).arg(static_cast<int>(action)));

    return true;
}

OSStatus GlobalHotkeyManager::hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void* userData)
{
    GlobalHotkeyManager* manager = static_cast<GlobalHotkeyManager*>(userData);

    EventHotKeyID hotkeyId;
    OSStatus status = GetEventParameter(
        event,
        kEventParamDirectObject,
        typeEventHotKeyID,
        nullptr,
        sizeof(hotkeyId),
        nullptr,
        &hotkeyId
    );

    if (status != noErr) {
        return status;
    }

    // Verify signature
    if (hotkeyId.signature != kHotkeySignature) {
        return eventNotHandledErr;
    }

    // Find and emit action
    if (manager->m_hotkeys.contains(hotkeyId.id)) {
        HotkeyAction action = manager->m_hotkeys[hotkeyId.id].action;
        LOG_DEBUG(QString("Global hotkey pressed: id=%1, action=%2").arg(hotkeyId.id).arg(static_cast<int>(action)));
        emit manager->hotkeyPressed(action);
        return noErr;
    }

    return eventNotHandledErr;
}

#else // Non-macOS platforms

GlobalHotkeyManager::GlobalHotkeyManager(QObject *parent)
    : QObject(parent)
{
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
}

bool GlobalHotkeyManager::registerHotkeys()
{
    LOG_WARNING("Global hotkeys are only supported on macOS");
    return false;
}

void GlobalHotkeyManager::unregisterHotkeys()
{
}

#endif // Q_OS_MAC
