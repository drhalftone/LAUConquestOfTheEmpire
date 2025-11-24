#include <QApplication>
#include "mapeditorwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Conquest Map Editor");
    app.setOrganizationName("LAU");

    MapEditorWindow window;
    window.show();

    return app.exec();
}
