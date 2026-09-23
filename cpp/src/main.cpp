#include "core/I18n.h"
#include "ui/MainWindow.h"

#include <QApplication>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GEDIProcessDesktopCpp"));
    ui::MainWindow w;
    w.show();
    return app.exec();
}
