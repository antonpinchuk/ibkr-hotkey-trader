#include "toastnotification.h"
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>

QMap<QString, ToastNotification*> ToastNotification::s_activeToasts;

ToastNotification::ToastNotification(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_type(Info)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMinimumHeight(60);
    setMinimumWidth(300);
    setMaximumWidth(500);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setTextFormat(Qt::PlainText);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->setContentsMargins(15, 10, 15, 10);

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

    ToastNotification *toast = new ToastNotification(parent);
    toast->showMessage(message, type);
    s_activeToasts[message] = toast;
}

void ToastNotification::showMessage(const QString &message, Type type)
{
    m_type = type;
    m_message = message;
    m_label->setText(message);

    QString bgColor;
    QString textColor = "white";
    QString borderColor;

    switch (type) {
    case Info:
        bgColor = "#2196F3"; // Blue
        borderColor = "#1976D2";
        break;
    case Warning:
        bgColor = "#FFC107"; // Yellow
        borderColor = "#FFA000";
        textColor = "#333333";
        break;
    case Error:
        bgColor = "#F44336"; // Red
        borderColor = "#D32F2F";
        break;
    }

    setStyleSheet(QString(
        "ToastNotification { "
        "   background-color: %1; "
        "   border: 2px solid %3; "
        "   border-radius: 8px; "
        "} "
        "QLabel { "
        "   color: %2; "
        "   font-size: 13px; "
        "   background-color: transparent; "
        "}"
    ).arg(bgColor, textColor, borderColor));

    adjustSize();

    // Position at bottom-right of parent
    if (parentWidget()) {
        QRect parentRect = parentWidget()->geometry();
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
}

void ToastNotification::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    fadeOut();
}
