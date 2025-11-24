QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = MapEditor

# macOS application icon
ICON = MapEditor.icns

SOURCES += \
    mapeditor_main.cpp \
    mapeditorwindow.cpp \
    editablegridwidget.cpp

HEADERS += \
    mapeditorwindow.h \
    editablegridwidget.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
