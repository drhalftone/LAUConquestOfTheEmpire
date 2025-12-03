QT       += core gui multimedia opengl openglwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Uncomment to use OpenGL-based map widget instead of grid-based widget
# NOTE: OpenGL map requires refactoring PlayerInfoWidget, CombatDialog, AIPlayer etc.
# to use GameMapWidget interface instead of MapWidget-specific methods.
DEFINES += USE_OPENGL_MAP

# Common sources (always included)
SOURCES += \
    main.cpp \
    mapgraph.cpp \
    gamepiece.cpp \
    player.cpp \
    building.cpp \
    gamelog.cpp \
    ai/reachabilitycalculator.cpp \
    ai/aidecisionmaker.cpp \
    ai/moveenumerator.cpp

# Common headers (always included)
HEADERS += \
    mapgraph.h \
    gamepiece.h \
    player.h \
    building.h \
    common.h \
    gamelog.h \
    ai/reachabilitycalculator.h \
    ai/aidecisionmaker.h \
    ai/moveenumerator.h

# Conditional compilation based on map type
contains(DEFINES, USE_OPENGL_MAP) {
    message("Using OpenGL map widget")
    SOURCES += \
        gamemapwidget.cpp \
        playerinfowidget.cpp \
        combatdialog.cpp \
        troopselectiondialog.cpp \
        purchasedialog.cpp \
        citydestructiondialog.cpp \
        laurollingdiewidget.cpp \
        aiplayer.cpp \
        aidebugwidget.cpp \
        moveenumeratorwidget.cpp
    HEADERS += \
        gamemapwidget.h \
        playerinfowidget.h \
        combatdialog.h \
        troopselectiondialog.h \
        purchasedialog.h \
        citydestructiondialog.h \
        laurollingdiewidget.h \
        aiplayer.h \
        aidebugwidget.h \
        moveenumeratorwidget.h
} else {
    message("Using grid-based map widget")
    SOURCES += \
        mapwidget.cpp \
        scorewindow.cpp \
        walletwindow.cpp \
        purchasedialog.cpp \
        playerinfowidget.cpp \
        troopselectiondialog.cpp \
        combatdialog.cpp \
        citydestructiondialog.cpp \
        laurollingdiewidget.cpp \
        aiplayer.cpp \
        aidebugwidget.cpp

    HEADERS += \
        mapwidget.h \
        scorewindow.h \
        walletwindow.h \
        purchasedialog.h \
        playerinfowidget.h \
        troopselectiondialog.h \
        combatdialog.h \
        citydestructiondialog.h \
        laurollingdiewidget.h \
        aiplayer.h \
        aidebugwidget.h
}

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
