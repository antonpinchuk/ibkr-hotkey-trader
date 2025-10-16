#include "utils/systemtraymanager.h"
#include "utils/logger.h"

#ifdef Q_OS_MAC
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
    , m_statusItem(nullptr)
    , m_isBlinking(false)
    , m_blinkVisible(true)
    , m_blinkTimer(new QTimer(this))
{
    createStatusItem();

    // Setup blink timer (1 second period = 0.5s on, 0.5s off)
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, &SystemTrayManager::onBlinkTimer);

    LOG_INFO("SystemTrayManager initialized");
}

SystemTrayManager::~SystemTrayManager()
{
    destroyStatusItem();
    LOG_INFO("SystemTrayManager destroyed");
}

void SystemTrayManager::createStatusItem()
{
    @autoreleasepool {
        // Create status bar item
        NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
        m_statusItem = [[statusBar statusItemWithLength:NSVariableStatusItemLength] retain];

        if (m_statusItem) {
            // Set initial title (empty)
            m_statusItem.button.title = @"";

            // Use monospaced font (SF Mono or Menlo)
            NSFont* monoFont = [NSFont monospacedSystemFontOfSize:13.0 weight:NSFontWeightRegular];
            if (monoFont) {
                NSMutableAttributedString* attrString = [[NSMutableAttributedString alloc] initWithString:@""];
                [attrString addAttribute:NSFontAttributeName value:monoFont range:NSMakeRange(0, 0)];
                m_statusItem.button.attributedTitle = attrString;
                [attrString release];
            }

            // Try to load tray icon from app bundle
            NSBundle* bundle = [NSBundle mainBundle];
            NSString* iconPath = [bundle pathForResource:@"tray-icon" ofType:@"icns"];

            if (iconPath) {
                NSImage* icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
                if (icon) {
                    // Set explicit size for status bar (18x18 recommended for modern macOS)
                    [icon setSize:NSMakeSize(18, 18)];
                    [icon setTemplate:YES]; // Makes it adapt to light/dark mode
                    m_statusItem.button.image = icon;
                    [icon release];
                }
            } else {
                LOG_WARNING("Tray icon not found in bundle, trying fallback");
                // Fallback to main app icon
                iconPath = [bundle pathForResource:@"icon" ofType:@"icns"];
                if (iconPath) {
                    NSImage* icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
                    if (icon) {
                        [icon setSize:NSMakeSize(18, 18)];
                        [icon setTemplate:YES];
                        m_statusItem.button.image = icon;
                        [icon release];
                    }
                }
            }

            LOG_INFO("Status bar item created");
        } else {
            LOG_ERROR("Failed to create status bar item");
        }
    }
}

void SystemTrayManager::destroyStatusItem()
{
    @autoreleasepool {
        if (m_statusItem) {
            NSStatusBar* statusBar = [NSStatusBar systemStatusBar];
            [statusBar removeStatusItem:m_statusItem];
            [m_statusItem release];
            m_statusItem = nullptr;
            LOG_INFO("Status bar item removed");
        }
    }
}

void SystemTrayManager::setTickerSymbol(const QString& symbol)
{
    m_tickerSymbol = symbol;
    updateTrayDisplay();
}

void SystemTrayManager::startBlinking()
{
    if (!m_isBlinking) {
        m_isBlinking = true;
        m_blinkVisible = true;
        m_blinkTimer->start();
    }
}

void SystemTrayManager::stopBlinking()
{
    if (m_isBlinking) {
        m_isBlinking = false;
        m_blinkVisible = true;
        m_blinkTimer->stop();
        updateTrayDisplay(); // Ensure text is visible when stopping
    }
}

void SystemTrayManager::onBlinkTimer()
{
    m_blinkVisible = !m_blinkVisible;
    updateTrayDisplay();
}

void SystemTrayManager::updateTrayDisplay()
{
    @autoreleasepool {
        if (!m_statusItem) {
            return;
        }

        QString displayText;
        if (m_tickerSymbol.isEmpty()) {
            displayText = ""; // Empty string when no ticker
        } else {
            // Show text only if blinking is off or if in visible phase
            if (!m_isBlinking || m_blinkVisible) {
                displayText = m_tickerSymbol;
            } else {
                // Replace text with spaces to preserve width during blink
                displayText = QString(m_tickerSymbol.length(), ' ');
            }
        }

        // Create attributed string with monospaced font
        NSString* nsTitle = displayText.toNSString();
        NSFont* monoFont = [NSFont monospacedSystemFontOfSize:13.0 weight:NSFontWeightRegular];

        NSMutableAttributedString* attrString = [[NSMutableAttributedString alloc] initWithString:nsTitle];
        [attrString addAttribute:NSFontAttributeName
                           value:monoFont
                           range:NSMakeRange(0, [nsTitle length])];

        m_statusItem.button.attributedTitle = attrString;
        [attrString release];
    }
}

#else // Non-macOS platforms

SystemTrayManager::SystemTrayManager(QObject *parent)
    : QObject(parent)
    , m_isBlinking(false)
    , m_blinkVisible(true)
    , m_blinkTimer(new QTimer(this))
{
    LOG_WARNING("SystemTrayManager is only supported on macOS");
}

SystemTrayManager::~SystemTrayManager()
{
}

void SystemTrayManager::createStatusItem()
{
}

void SystemTrayManager::destroyStatusItem()
{
}

void SystemTrayManager::setTickerSymbol(const QString& symbol)
{
    m_tickerSymbol = symbol;
}

void SystemTrayManager::startBlinking()
{
}

void SystemTrayManager::stopBlinking()
{
}

void SystemTrayManager::onBlinkTimer()
{
}

void SystemTrayManager::updateTrayDisplay()
{
}

#endif // Q_OS_MAC
