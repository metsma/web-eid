/*
 * Copyright (C) 2017 Martin Paljak
 */

#include "autostart.h"
#include <QFile>
#include "Logger.h"

#ifdef Q_OS_MACOS
#include <QProcess>
#include <QUrl>
#include <ServiceManagement/ServiceManagement.h>
#endif

// For Mac OS X: http://hints.macworld.com/article.php?story=20111226075701552
// Current bundle path: http://stackoverflow.com/questions/3489405/qt-accessing-the-bundle-path

bool StartAtLoginHelper::isEnabled() {
    bool enabled = false;
#if defined(Q_OS_LINUX)
    // chekc for presence of desktop entry
    if (QFile("/etc/xdg/autostart/web-eid-service.desktop").exists())
        return true;
    // TODO: if the following file exists as well, it means user has overriden the default
    // startup script, eg disabled (X-MATE-Autostart-enabled=false or something similar)
    //if (QFile(QDir::homePath().filePath(".config/autostart/web-eid-service.desktop")))
#elif defined(Q_OS_MACOS)
    QProcess osascript;
    osascript.start("/usr/bin/osascript", {"-e", "tell application \"System Events\" to get the name of every login item"});
    osascript.waitForFinished(); // we assume fast execution
    QString output(osascript.readAllStandardOutput());
    _log("Output is: %s", qPrintable(output));
    enabled = output.simplified().split(",").contains(" Web eID");
    _log("Enabled: %d", enabled);
#elif defined(Q_OS_WIN32)
    // Check registry entry and if it addresses *this* instance of the app
    // QCoreApplication::applicationFilePath()
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
    QProcess osascript;
    if (enabled) {
        osascript.start("/usr/bin/osascript", {"-e", QString("tell application \"System Events\" to make login item at end with properties {path:\"%1\", hidden:false}").arg(bundlepath)});
    } else {
        osascript.start("/usr/bin/osascript", {"-e", "tell application \"System Events\" to delete login item \"Web eID\""});
    }
    osascript.waitForFinished(); // we assume fast execution
    QString output(osascript.readAllStandardOutput());
    _log("Output is: %s", qPrintable(output));
#elif defined(Q_OS_WIN32)
    // Add registry entry. Utilizing  QCoreApplication::applicationFilePath()
#else
#error "Unsupported platform"
#endif


    return true;
}
