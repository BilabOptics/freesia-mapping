QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = freesia-2.0.1
TEMPLATE = app

RC_FILE = app.rc

Release:DESTDIR = ../bin/Release
Debug:DESTDIR = ../bin/Debug

DEFINES += QT_DEPRECATED_WARNINGS
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_LFLAGS_RELEASE += /MAP
QMAKE_CXXFLAGS_RELEASE += /Zi
QMAKE_LFLAGS_RELEASE += /debug /opt:ref

include(dependencies.pri)

SOURCES += main.cpp\
        mainwindow.cpp \
    common.cpp \
    controlpanel.cpp \
    menubar.cpp \
    contrastadjuster.cpp \
    statusbar.cpp \
    brainregionpanel.cpp \
    stackslider.cpp \
    brainregionmodel.cpp \
    volumeviewer.cpp \
    viewerpanel.cpp \
    sectionviewer.cpp \
    importexportdialog.cpp

HEADERS  += mainwindow.h \
    common.h \
    controlpanel.h \
    menubar.h \
    contrastadjuster.h \
    statusbar.h \
    brainregionpanel.h \
    stackslider.h \
    brainregionmodel.h \
    volumeviewer.h \
    viewerpanel.h \
    sectionviewer.h \
    importexportdialog.h

RESOURCES += \
    assets.qrc
