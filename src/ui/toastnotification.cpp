#include "ui/toastnotification.h"
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

QMap<QString, ToastNotification*> ToastNotification::s_activeToasts;
QList<ToastNotification*> ToastNotification::s_toastList;

ToastNotification::ToastNotification(QWidget *parent)
    : QWidget(parent) // Child widget, no separate window
    , m_type(Info)
{
    // Ensure toast doesn't steal focus or activate parent window
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);

    setFixedWidth(500);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_label->setTextFormat(Qt::PlainText);
    m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    m_label->setFixedWidth(470);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(5000); // 5 seconds
    connect(m_timer, &QTimer::timeout, this, &ToastNotification::fadeOut);

    connect(this, &QWidget::destroyed, this, &ToastNotification::onDestroyed);
}

void ToastNotification::show(QWidget *parent, const QString &message, Type type)
{
    // Check if toast with same message is already showing
    if (s_activeToasts.contains(message)) {
        // Reset timer on existing toast
        ToastNotification *existing = s_activeToasts[message];
        if (existing && existing->isVisible()) {
            existing->m_timer->stop();
            existing->m_timer->start();
            return;
        }
    }

    // Close all existing toasts to avoid clutter
    closeAllToasts();

    ToastNotification *toast = new ToastNotification(parent);
    toast->showMessage(message, type);
    s_activeToasts[message] = toast;
    s_toastList.append(toast);
}

void ToastNotification::closeAllToasts()
{
    // Close all existing toasts
    for (ToastNotification *toast : s_toastList) {
        if (toast && toast->isVisible()) {
            toast->close();
        }
    }
    s_toastList.clear();
}

void ToastNotification::showMessage(const QString &message, Type type)
{
    m_type = type;
    m_message = message;
    m_label->setText(message);

    QString textColor = "white";

    switch (type) {
    case Info:
        m_bgColor = "#2196F3"; // Blue
        m_borderColor = "#1976D2";
        break;
    case Warning:
        m_bgColor = "#FFC107"; // Yellow
        m_borderColor = "#FFA000";
        textColor = "#333333";
        break;
    case Error:
        m_bgColor = "#F44336"; // Red
        m_borderColor = "#D32F2F";
        break;
    }

    setStyleSheet(QString(
        "QLabel { "
        "   color: %1; "
        "   font-size: 13px; "
        "   background-color: transparent; "
        "   padding: 0px; "
        "   border: none; "
        "}"
    ).arg(textColor));

    // Calculate proper height based on text
    m_label->adjustSize();
    int labelHeight = m_label->sizeHint().height();
    int totalHeight = labelHeight + 30; // 30 for margins
    setFixedHeight(totalHeight);

    // Position at bottom-right of parent (using parent's rect, not geometry)
    if (parentWidget()) {
        QRect parentRect = parentWidget()->rect();
        int x = parentRect.right() - width() - 20;
        int y = parentRect.bottom() - height() - 20;
        move(x, y);
    }

    QWidget::show();
    raise();
    m_timer->start();
}

void ToastNotification::fadeOut()
{
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);

    QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(1.0);
    animation->setEndValue(0.0);
    animation->setEasingCurve(QEasingCurve::OutQuad);

    connect(animation, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastNotification::onDestroyed()
{
    s_activeToasts.remove(m_message);
    s_toastList.removeAll(this);
}

void ToastNotification::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    fadeOut();
}

void ToastNotification::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded rectangle background
    QPainterPath path;
    path.addRoundedRect(rect(), 8, 8);

    // Fill background
    painter.fillPath(path, QColor(m_bgColor));

    // Draw border
    painter.setPen(QPen(QColor(m_borderColor), 2));
    painter.drawPath(path);
}
