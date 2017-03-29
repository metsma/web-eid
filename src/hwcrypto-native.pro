TEMPLATE = app
isEmpty(VERSION) {
    include(../VERSION.mk)
    BUILD_NUMBER = $$(BUILD_NUMBER)
    isEmpty(BUILD_NUMBER) BUILD_NUMBER = 1
    VERSION=$$VERSION"."$$BUILD_NUMBER
}
CONFIG += console c++11
QT += widgets network
RC_ICONS = ../artwork/win_icon.ico
macx {
    LIBS += -framework PCSC
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
    QMAKE_INFO_PLIST += Info.plist
    CONFIG += app_bundle
}
unix:!macx: {
    PKGCONFIG += libpcsclite
    CONFIG += link_pkgconfig
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
}
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    Logger.cpp \
    modulemap.cpp \
    pcsc.cpp \
    pkcs11module.cpp \
    qt/chrome-host.cpp \
    qt/authenticate.cpp \
    qt/sign.cpp \
    qt/qt_signer.cpp \
    qt/qt_certselect.cpp \
    qt/qt_pcsc.cpp
HEADERS += *.h qt/*.h
RESOURCES += qt/hwcrypto-native.qrc qt/translations/strings.qrc
TRANSLATIONS = qt/translations/strings_et.ts qt/translations/strings_ru.ts

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
lrelease.commands = $$QMAKE_LRELEASE hwcrypto-native.pro
PRE_TARGETDEPS += lrelease
QMAKE_EXTRA_TARGETS += lrelease
