#include <QApplication>
#include <QStringList>

#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("napari-cpp"));
    QApplication::setOrganizationName(QStringLiteral("napari-cpp"));

    napari_cpp::MainWindow window;
    window.show();

    const QStringList args = app.arguments();
    if (args.size() > 1) {
        window.openPaths(args.mid(1));
    }

    return app.exec();
}
