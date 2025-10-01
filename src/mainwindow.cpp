#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("IBKR Hotkey Trader");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
}