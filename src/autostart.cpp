/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "autostart.h"
#include <QFile>
#include "Logger.h"

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <QUrl>
QString osascript(QString scpt); // macosxui.mm
#endif
#ifdef Q_OS_WIN
#include <QDir>
#include <QSettings>
#include <QCoreApplication>
#endif

// For Mac OS X: http://hints.macworld.com/article.php?story=20111226075701552
// Current bundle path: http://stackoverflow.com/questions/3489405/qt-accessing-the-bundle-path

bool StartAtLoginHelper::isEnabled() {
    bool enabled = false;
#if defined(Q_OS_LINUX)
    // chekc for presence of desktop entry
    if (QFile("/etc/xdg/autostart/web-eid-service.desktop").exists())
        enabled = true;
    // TODO: if the following file exists as well, it means user has overriden the default
    // startup script, eg disabled (X-MATE-Autostart-enabled=false or something similar)
    //if (QFile(QDir::homePath().filePath(".config/autostart/web-eid-service.desktop")))
#elif defined(Q_OS_MACOS)
    CFURLRef url = (CFURLRef)CFAutorelease((CFURLRef)CFBundleCopyBundleURL(CFBundleGetMainBundle()));
    QString bundlepath = QUrl::fromCFURL(url).path();
    // XXX: we cast the list to a string in applescript, for simple osascript() function
    QString names = osascript("set text item delimiters to \",\"\ntell application \"System Events\" to get the name of every login item as text");
    QString paths = osascript("set text item delimiters to \",\"\ntell application \"System Events\" to get the path of every login item as text");
    // Check that path matches
    enabled = names.simplified().split(",").contains("Web eID") && paths.split(",").contains(bundlepath);
#elif defined(Q_OS_WIN32)
    QSettings startup("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    enabled = startup.value("Web eID").toString() == QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
#else
#error "Unsupported platform"
#endif
    return enabled;
}

bool StartAtLoginHelper::setEnabled(bool enabled) {
#if defined(Q_OS_LINUX)
    // disable: add a file to ~/.config with "disabled" flag.
    // enable: make sure global autostart exists and user file is removed
#elif defined(Q_OS_MACOS)
    // Get current bundle path
    CFURLRef url = (CFURLRef)CFAutorelease((CFURLRef)CFBundleCopyBundleURL(CFBundleGetMainBundle()));
    QString bundlepath = QUrl::fromCFURL(url).path();
    _log("Current bundle is: %s", qPrintable(bundlepath));
    if (enabled) {
        // Delete any existing items before adding a new one.
        setEnabled(false);
        _log("result: %s", qPrintable(osascript(QString("tell application \"System Events\" to make login item at end with properties {name:\"Web eID\", path:\"%1\", hidden:false}").arg(bundlepath))));
    } else {
        _log("result: %s", qPrintable(osascript("tell application \"System Events\" to delete login item \"Web eID\"")));
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
