#include "toastnotification.h"
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>

ToastNotification::ToastNotification(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_type(Info)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedHeight(60);
    setMinimumWidth(300);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    layout->setContentsMargins(15, 10, 15, 10);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(10000); // 10 seconds
    connect(m_timer, &QTimer::timeout, this, &ToastNotification::fadeOut);
}

void ToastNotification::show(QWidget *parent, const QString &message, Type type)
{
    ToastNotification *toast = new ToastNotification(parent);
    toast->showMessage(message, type);
}

void ToastNotification::showMessage(const QString &message, Type type)
{
    m_type = type;
    m_label->setText(message);

    QString bgColor;
    QString textColor = "white";

    switch (type) {
    case Info:
        bgColor = "#2196F3"; // Blue
        break;
    case Warning:
        bgColor = "#FFC107"; // Yellow
        textColor = "black";
        break;
    case Error:
        bgColor = "#F44336"; // Red
        break;
    }

    setStyleSheet(QString(
        "ToastNotification { "
        "   background-color: %1; "
        "   border-radius: 8px; "
        "} "
        "QLabel { "
        "   color: %2; "
        "   font-size: 13px; "
        "}"
    ).arg(bgColor, textColor));

    adjustSize();

    // Position at bottom-right of parent
    if (parentWidget()) {
        QRect parentRect = parentWidget()->geometry();
        int x = parentRect.right() - width() - 20;
        int y = parentRect.bottom() - height() - 20;
        move(x, y);
    }

    QWidget::show();
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

void ToastNotification::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    fadeOut();
}
