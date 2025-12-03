QT += core gui widgets openglwidgets multimedia

CONFIG += c++17

TARGET = MapViewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mapviewerwidget.cpp

HEADERS += \
    mapviewerwidget.h

RESOURCES += \
    resources.qrc

macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}
