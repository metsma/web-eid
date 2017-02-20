TEMPLATE = app
CONFIG += console c++11 link_pkgconfig
CONFIG -= app_bundle
QT += widgets network
isEmpty(VERSION):VERSION=1.0.4.0
PKGCONFIG += openssl libpcsclite
INCLUDEPATH += ../host-shared
LIBS += -ldl
DEFINES += VERSION=\\\"$$VERSION\\\"
SOURCES += \
    ../host-shared/Labels.cpp \
    ../host-shared/Logger.cpp \
    ../host-shared/BinaryUtils.cpp \
    ../host-shared/AtrFetcher.cpp \
    ../host-shared/PKCS11Path.cpp \
    chrome-host.cpp
HEADERS += *.h ../host-shared/*.h
RESOURCES += hwcrypto-native.qrc
target.path = /usr/bin
hostconf.path = /etc/opt/chrome/native-messaging-hosts
hostconf.files += org.hwcrypto.native.json
# https://bugzilla.mozilla.org/show_bug.cgi?id=1318461
ffconf.path = /usr/lib/mozilla/native-messaging-hosts
ffconf.files += ff/org.hwcrypto.native.json
extension.path = /opt/google/chrome/extensions
extension.files += ../fmpfihjoladdfajbnkdfocnbcehjpogi.json
ffextension.path = /usr/share/mozilla/extensions/{ec8030f7-c20a-464f-9b0e-13a3a9e97384}
ffextension.files += ../{443830f0-1fff-4f9a-aa1e-444bafbc7319}.xpi
INSTALLS += target hostconf ffconf extension ffextension
