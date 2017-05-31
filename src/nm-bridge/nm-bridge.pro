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
CONFIG -= app_bundle
QT += network
QT -= gui
RC_ICONS = ../../artwork/win_icon.ico
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
}
win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
}
TARGET = web-eid-bridge
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += ..\debuglog.cpp nm-bridge.cpp
