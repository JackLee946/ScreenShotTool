QT += core gui widgets
CONFIG += c++11
TARGET = ScreenShotTool
TEMPLATE = app

SOURCES += main.cpp mainwindow.cpp screenshotwidget.cpp
HEADERS += mainwindow.h screenshotwidget.h

win32 {
    LIBS += -luser32
    DEFINES += NOMINMAX
}
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}
