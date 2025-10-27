#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QMap>

class ToastNotification : public QWidget
{
    Q_OBJECT

public:
    enum Type {
        Info,
        Warning,
        Error
    };

    explicit ToastNotification(QWidget *parent = nullptr);

    static void show(QWidget *parent, const QString &message, Type type = Info);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void fadeOut();
    void onDestroyed();

private:
    void showMessage(const QString &message, Type type);
    static void closeAllToasts();

    static QMap<QString, ToastNotification*> s_activeToasts;
    static QList<ToastNotification*> s_toastList;

    QLabel *m_label;
    QTimer *m_timer;
    Type m_type;
    QString m_message;
    QString m_bgColor;
    QString m_borderColor;
};

#endif // TOASTNOTIFICATION_H
