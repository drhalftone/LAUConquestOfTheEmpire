#include <QApplication>
#include <QSurfaceFormat>
#include "mapviewerwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Request OpenGL 3.3 Core Profile
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);  // Anti-aliasing
    QSurfaceFormat::setDefaultFormat(format);

    MapViewerWidget widget;
    widget.setWindowTitle("Map Viewer");
    widget.resize(1280, 800);
    widget.show();

    return app.exec();
}
