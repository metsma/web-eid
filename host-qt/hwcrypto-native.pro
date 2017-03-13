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
    SOURCES += ../host-windows/WinCertSelect.cpp ../host-windows/WinSigner.cpp
    HEADERS += ../host-windows/WinCertSelect.h ../host-windows/WinSigner.h
    INCLUDEPATH += ../host-windows
    QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
}
INCLUDEPATH += ../host-shared
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    ../host-shared/Logger.cpp \
    ../host-shared/modulemap.cpp \
    ../host-shared/pcsc.cpp \
    ../host-shared/pkcs11module.cpp \
    chrome-host.cpp \
    authenticate.cpp \
    sign.cpp \
    qt_signer.cpp \
    qt_certselect.cpp
HEADERS += *.h ../host-shared/*.h
RESOURCES += hwcrypto-native.qrc translations/strings.qrc
TRANSLATIONS = translations/strings_et.ts translations/strings_ru.ts

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}
lrelease.commands = $$QMAKE_LRELEASE hwcrypto-native.pro
PRE_TARGETDEPS += lrelease
QMAKE_EXTRA_TARGETS += lrelease