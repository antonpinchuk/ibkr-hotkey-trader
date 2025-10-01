#include <QApplication>
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("IBKR Hotkey Trader");
    app.setApplicationVersion("0.1");
    app.setOrganizationName("Kinect.PRO");
    app.setOrganizationDomain("kinect-pro.com");

    MainWindow window;
    window.show();

    return app.exec();
}
