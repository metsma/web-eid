TEMPLATE = app
isEmpty(VERSION):VERSION=1.0.4.0
CONFIG += console c++11 release
QT += widgets network
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
    LIBS += winscard.lib ncrypt.lib crypt32.lib cryptui.lib Advapi32.lib
    SOURCES += ../host-windows/WinCertSelect.cpp ../host-windows/WinSigner.cpp
    HEADERS += ../host-windows/WinCertSelect.h ../host-windows/WinSigner.h
    INCLUDEPATH += ../host-windows
}
INCLUDEPATH += ../host-shared
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    ../host-shared/Labels.cpp \
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
RESOURCES += hwcrypto-native.qrc
