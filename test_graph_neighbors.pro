QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Test executable name
TARGET = test_graph_neighbors

# This is a test application
TEMPLATE = app

# Include all source files needed for MapWidget
SOURCES += \
    test_graph_neighbors.cpp \
    mapwidget.cpp \
    mapgraph.cpp \
    gamepiece.cpp \
    player.cpp \
    building.cpp \
    scorewindow.cpp \
    walletwindow.cpp \
    purchasedialog.cpp \
    placementdialog.cpp \
    playerinfowidget.cpp \
    troopselectiondialog.cpp \
    combatdialog.cpp \
    citydestructiondialog.cpp

HEADERS += \
    mapwidget.h \
    mapgraph.h \
    gamepiece.h \
    player.h \
    building.h \
    scorewindow.h \
    walletwindow.h \
    purchasedialog.h \
    placementdialog.h \
    playerinfowidget.h \
    common.h \
    troopselectiondialog.h \
    combatdialog.h \
    citydestructiondialog.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
