QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Test executable name
TARGET = test_graph_neighbors

# This is a test application
TEMPLATE = app

# Minimal source files needed for the test
# Note: MapWidget depends on GamePiece and Player, so we need their full implementations
SOURCES += \
    test_graph_neighbors.cpp \
    mapwidget.cpp \
    mapgraph.cpp \
    gamepiece.cpp \
    player.cpp \
    building.cpp

# Minimal headers
HEADERS += \
    mapwidget.h \
    mapgraph.h \
    gamepiece.h \
    player.h \
    building.h \
    common.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
