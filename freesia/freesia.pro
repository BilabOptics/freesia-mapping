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

INCLUDEPATH += $$(OPENCV_DIR)\include
Release: LIBS += -L$$(OPENCV_DIR)\x64\vc14\lib -lopencv_world340
Debug: LIBS += -L$$(OPENCV_DIR)\x64\vc14\lib -lopencv_world340d

INCLUDEPATH += $$(NN_DIR)\nn
Release: LIBS += -L$$(NN_DIR)\x64\Release -lnn
Debug: LIBS += -L$$(NN_DIR)\x64\Debug -lnn

INCLUDEPATH += $$(LIB_PATH)\VTK-8.2-kits\include\vtk-8.2
LIBS += -L$$(LIB_PATH)\VTK-8.2-kits\lib
LIBS += -lvtkCommon-8.2 -lvtkFilters-8.2 -lvtkGUISupportQt-8.2 -lvtkGeovisCore-8.2 -lvtkIO-8.2 -lvtkIOImport-8.2 -lvtkImaging-8.2 -lvtkInteraction-8.2 -lvtkOpenGL-8.2 -lvtkRendering-8.2 -lvtkRenderingQt-8.2

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
