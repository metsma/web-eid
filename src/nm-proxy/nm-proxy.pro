OBJECTS_DIR = build
MOC_DIR = build
RCC_DIR = build
TEMPLATE = app
isEmpty(VERSION) {
    include(../../VERSION.mk)
    BUILD_NUMBER = $$(BUILD_NUMBER)
    isEmpty(BUILD_NUMBER) BUILD_NUMBER = 1
    VERSION=$$VERSION"."$$BUILD_NUMBER
}
CONFIG += console c++11
QT += network websockets
RC_ICONS = ../artwork/win_icon.ico
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
}
win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
    LIBS += winscard.lib ncrypt.lib crypt32.lib cryptui.lib Advapi32.lib
    SOURCES += win/WinCertSelect.cpp win/WinSigner.cpp
    HEADERS += win/WinCertSelect.h win/WinSigner.h
    INCLUDEPATH += win
    QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
}
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    ..\Logger.cpp \
    nm-proxy.cpp

HEADERS += $$files(*.h)
