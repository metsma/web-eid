OBJECTS_DIR = build
MOC_DIR = build
RCC_DIR = build
TEMPLATE = app
isEmpty(VERSION) {
    include(../VERSION.mk)
    BUILD_NUMBER = $$(BUILD_NUMBER)
    isEmpty(BUILD_NUMBER) BUILD_NUMBER = 1
    VERSION=$$VERSION"."$$BUILD_NUMBER
}
CONFIG += c++11
QT += widgets network websockets
RC_ICONS = ../artwork/win_icon.ico
macx {
    LIBS += -framework PCSC -framework ServiceManagement -framework CoreFoundation
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    ICON = ../artwork/mac.icns
    QMAKE_INFO_PLIST += Info.plist
    CONFIG += app_bundle
    TARGET = "Web eID"
    QMAKE_POST_LINK += "mkdir -p \"$${TARGET}.app/Contents/Library/LoginItems\" && cp -r login/login.app \"$${TARGET}.app/Contents/Library/LoginItems\""
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
    SOURCES += win/WinCertSelect.cpp win/WinSigner.cpp
    HEADERS += win/WinCertSelect.h win/WinSigner.h
    INCLUDEPATH += win
    QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    TARGET = "Web-eID"
}
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    Logger.cpp \
    modulemap.cpp \
    pcsc.cpp \
    pkcs11module.cpp \
    qt/main.cpp \
    qt/qpcsc.cpp \
    qt/qpki.cpp \
    qt/qt_pki.cpp \
    qt/autostart.cpp \
    qt/context.cpp
INCLUDEPATH += qt
HEADERS += $$files(*.h) $$files(qt/*.h) $$files(qt/dialogs/*.h)
RESOURCES += qt/web-eid.qrc qt/translations/strings.qrc
TRANSLATIONS = qt/translations/strings_et.ts qt/translations/strings_ru.ts

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
lrelease.commands = $$QMAKE_LRELEASE $$PWD/src.pro
PRE_TARGETDEPS += lrelease
QMAKE_EXTRA_TARGETS += lrelease
