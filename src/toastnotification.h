#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include <QWidget>
#include <QLabel>
#include <QTimer>

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

private slots:
    void fadeOut();

private:
    void showMessage(const QString &message, Type type);

    QLabel *m_label;
    QTimer *m_timer;
    Type m_type;
};

#endif // TOASTNOTIFICATION_H
