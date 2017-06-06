lessThan(QT_MAJOR_VERSION, 5): error("requires Qt 5")
lessThan(QT_MINOR_VERSION, 6): error("requires Qt 5.6 or later")
include(../VERSION.mk)

OBJECTS_DIR = build
MOC_DIR = build
RCC_DIR = build
TEMPLATE = app
CONFIG += c++11
QT += widgets network websockets concurrent svg
RC_ICONS = ../artwork/win_icon.ico
macx {
    LIBS += -framework PCSC -framework ServiceManagement -framework CoreFoundation -framework AppKit
    QMAKE_OBJECTIVE_CFLAGS = -fobjc-arc
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    ICON = ../artwork/mac.icns
    QMAKE_INFO_PLIST += Info.plist
    CONFIG += app_bundle
    TARGET = "Web eID"
    SOURCES += dialogs/macosxui.mm
}
unix:!macx: {
    PKGCONFIG += libpcsclite
    CONFIG += link_pkgconfig debug
    TARGET = "web-eid"
}
unix {
    LIBS += -ldl
}
win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
    LIBS += winscard.lib ncrypt.lib crypt32.lib cryptui.lib Advapi32.lib
    SOURCES += qwincrypt.cpp
    HEADERS += qwincrypt.h
 #   QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    TARGET = "Web-eID"
    QMAKE_TARGET_COMPANY = "Web eID team"
    QMAKE_TARGET_PRODUCT = "Web eID"
    QMAKE_TARGET_DESCRIPTION = "Use your eID smart card on the Web"
    QMAKE_TARGET_COPYRIGHT = "(C) 2017 Martin Paljak"
}
DEFINES += VERSION=\\\"$$VERSION\\\"
DEFINES += "GIT_REVISION=\"\\\"$$system(git describe --tags --always)\\\"\""
SOURCES += \
    debuglog.cpp \
    modulemap.cpp \
    pkcs11module.cpp \
    main.cpp \
    qpcsc.cpp \
    qpki.cpp \
    autostart.cpp \
    webextension.cpp \
    context.cpp
HEADERS += $$files(*.h) $$files(dialogs/*.h)
RESOURCES += web-eid.qrc translations/strings.qrc
TRANSLATIONS = translations/strings_et.ts translations/strings_ru.ts

#should only be used if can be made to depend on source files
#isEmpty(QMAKE_LRELEASE) {
#    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\lrelease.exe
#    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
#}
#lrelease.commands = $$QMAKE_LRELEASE $$PWD/src.pro
#PRE_TARGETDEPS += lrelease
#QMAKE_EXTRA_TARGETS += lrelease
