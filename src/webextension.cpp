/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "webextension.h"
#include <QFile>
#include "Logger.h"

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <QUrl>
#endif

#include <QDir>
#include <QSettings>
#include <QCoreApplication>

bool WebExtensionHelper::isEnabled() {
    bool enabled = false;
#if defined(Q_OS_LINUX)
    // chekc for presence of desktop entry
    if (QFile("/etc/xdg/autostart/web-eid-service.desktop").exists())
        enabled = true;
    // TODO: if the following file exists as well, it means user has overriden the default
    // startup script, eg disabled (X-MATE-Autostart-enabled=false or something similar)
    //if (QFile(QDir::homePath().filePath(".config/autostart/web-eid-service.desktop")))
#elif defined(Q_OS_MACOS)
    // Check for Chrome and for Firefox
    if (QDir::home().exists("Library/Application Support/Google/Chrome/NativeMessagingHosts/com.web-eid.app.json")) {
        enabled = true;
    }
// ~/Library/Application Support/Google/Chrome/NativeMessagingHosts/com.my_company.my_application.json
// ~/Library/Application Support/Chromium/NativeMessagingHosts/com.my_company.my_application.json
#elif defined(Q_OS_WIN32)
    QSettings startup("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    enabled = startup.value("Web eID").toString() == QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
#else
#error "Unsupported platform"
#endif
    return enabled;
}

bool WebExtensionHelper::setEnabled(bool enabled) {
#if defined(Q_OS_LINUX)
    // disable: add a file to ~/.config with "disabled" flag.
    // enable: make sure global autostart exists and user file is removed
#elif defined(Q_OS_MACOS)
    QString nmpath = QDir(QCoreApplication::applicationDirPath()).filePath("web-eid-bridge");
    if (enabled) {
        // bundle files as resources
        //_log("result: %s", qPrintable(osascript(QString("tell application \"System Events\" to make login item at end with properties {name:\"Web eID\", path:\"%1\", hidden:false}").arg(bundlepath))));
    } else {
        //_log("result: %s", qPrintable(osascript("tell application \"System Events\" to delete login item \"Web eID\"")));
    }
#elif defined(Q_OS_WIN32)
    // Add registry entry. Utilizing  QCoreApplication::applicationFilePath()
    QSettings startup("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    if (enabled) {
        startup.setValue("Web eID", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        startup.remove("Web eID");
    }
#else
#error "Unsupported platform"
#endif


    return true;
}
