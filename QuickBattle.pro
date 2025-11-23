QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = QuickBattle

# macOS application icon
ICON = QuickBattle.icns

SOURCES += \
    quickbattle_main.cpp \
    quickbattlesplash.cpp \
    quickbattle_stubs.cpp \
    purchasedialog.cpp \
    combatdialog.cpp \
    gamepiece.cpp \
    player.cpp \
    building.cpp \
    laurollingdiewidget.cpp \
    mapwidget.cpp \
    mapgraph.cpp

HEADERS += \
    quickbattlesplash.h \
    purchasedialog.h \
    combatdialog.h \
    gamepiece.h \
    player.h \
    building.h \
    common.h \
    laurollingdiewidget.h \
    mapwidget.h \
    mapgraph.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
