#include <View/holefillwindow.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    view::HoleFillWindow w;
    w.show();

    return a.exec();
}