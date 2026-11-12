#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);   // 关闭窗口后留在托盘，不退出

    MainWindow w;
    w.show();
    return a.exec();
}
