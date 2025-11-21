QT      += core gui widgets

CONFIG  += c++17
TEMPLATE = app
DEFINES += QT_DEPRECATED_WARNINGS
CONFIG  += sdk_no_version_check

TARGET = LAUYahtzee

HEADERS += laurollingdiewidget.h \
           lauyahtzeewidget.h \
           lauscoresheetwidget.h

SOURCES += main.cpp \
           laurollingdiewidget.cpp \
           lauyahtzeewidget.cpp \
           lauscoresheetwidget.cpp

CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
