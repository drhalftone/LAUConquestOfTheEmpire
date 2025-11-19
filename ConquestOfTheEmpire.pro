QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    mapwidget.cpp \
    scorewindow.cpp \
    walletwindow.cpp \
    purchasedialog.cpp \
    placementdialog.cpp \
    gamepiece.cpp \
    player.cpp \
    building.cpp \
    playerinfowidget.cpp \
    troopselectiondialog.cpp \
    combatdialog.cpp \
    citydestructiondialog.cpp

HEADERS += \
    mainwindow.h \
    mapwidget.h \
    scorewindow.h \
    walletwindow.h \
    purchasedialog.h \
    placementdialog.h \
    gamepiece.h \
    player.h \
    building.h \
    playerinfowidget.h \
    common.h \
    troopselectiondialog.h \
    combatdialog.h \
    citydestructiondialog.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
