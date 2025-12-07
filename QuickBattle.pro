QT       += core gui multimedia openglwidgets widgets

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
    mapgraph.cpp \
    aiplayer.cpp \
    gamelog.cpp \
    gamemapwidget.cpp \
    moveenumeratorwidget.cpp \
    ai/combatsimulator.cpp \
    ai/aidecisionmaker.cpp \
    ai/reachabilitycalculator.cpp \
    ai/moveenumerator.cpp

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
    mapgraph.h \
    aiplayer.h \
    playerinfowidget.h \
    gamelog.h \
    gamemapwidget.h \
    moveenumeratorwidget.h \
    ai/combatsimulator.h \
    ai/aidecisionmaker.h \
    ai/reachabilitycalculator.h \
    ai/moveenumerator.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
